#include "FreeRTOS.h"
#include "task.h"
#include "Vision.h"
#include "vision_task.h"
#include "Servo.h"
#include "Pump.h"
#include "sensor_task.h"
#include "key_task.h"
#include "aim_task.h"

/*
 * 对靶喷药控制任务（自动作业模式CarMode=1下运行）：
 *
 * 状态机（两个阶段）：
 *   PATROL  巡逻搜索：直线行驶（近障安全停车），无目标时不喷
 *   TURN    车体转向：识别到目标后，差速原地旋转，把害虫对到画面x中心±10像素，
 *           对准后停车
 *   TRACK   舵机微调：停车不动舵机以外的东西，水平舵机调x、俯仰舵机调y，
 *           当x和y误差都在±10像素内 -> 开泵喷水；目标跟丢 -> 回PATROL
 *
 * 待机模式（CarMode=0）下本任务不接管任何执行器，退出时停车停喷。
 *
 * 装标定注意：
 *   VISION_CX/CY = MaixCAM模型输入分辨率的一半（320x240模型即160/120）
 *   AIM_YAW_SIGN / AIM_PITCH_SIGN：舵机方向（越调越偏就改成-1）
 *   TURN里左右轮赋值：车转向反了就交换两行
 */

#define AIM_TOL_X		10		//x方向对准容差（像素）
#define AIM_TOL_Y		10		//y方向对准容差（像素）
#define VISION_CX		160		//画面x中心（=模型输入宽/2）
#define VISION_CY		120		//画面y中心（=模型输入高/2）
#define PX_PER_DEG		10		//舵机1°对应画面像素数（视场角估算，用于比例步进）
#define AIM_YAW_SIGN	(+1)	//水平舵机方向符号（相机固定在车体则设0）
#define AIM_PITCH_SIGN	(+1)	//俯仰舵机方向符号
#define PATROL_DUTY		40		//巡逻搜索直行占空比（%）
#define TURN_DUTY		30		//转向对准占空比（%）
#define SPRAY_DUTY		70		//对准后喷水占空比（%）

typedef enum { AIM_PATROL = 0, AIM_TURN, AIM_TRACK } AimState;

volatile int8_t AimDriveL = 0;
volatile int8_t AimDriveR = 0;

/*角度限幅*/
static uint8_t Aim_ClampAngle(int16_t Angle)
{
	if (Angle < 0) Angle = 0;
	if (Angle > 180) Angle = 180;
	return (uint8_t)Angle;
}

/*误差像素->舵机步进角（比例，限幅1~5°）*/
static int16_t Aim_Step(int16_t Err)
{
	int16_t Step = Err / PX_PER_DEG;
	if (Step > 5) Step = 5;
	if (Step < -5) Step = -5;
	if (Step == 0) Step = (Err > 0) ? 1 : -1;
	return Step;
}

void AimTask(void *pvParameters)
{
	uint8_t WasAuto = 0;						//上一周期是否处于自动作业（用于退出清理）
	AimState State = AIM_PATROL;

	for (;;)
	{
		if (!CarMode)							//待机：不接管执行器
		{
			if (WasAuto)						//刚退出自动作业：停车停喷、复位状态
			{
				AimDriveL = AimDriveR = 0;
				Pump_SetDuty(0);
				State = AIM_PATROL;
				WasAuto = 0;
			}
			vTaskDelay(pdMS_TO_TICKS(20));
			continue;
		}
		WasAuto = 1;

		uint8_t Obstacle = ((SensorDistanceCm != 999 && SensorDistanceCm < 15)
							|| SensorIR1 || SensorIR2);

		if (!VisionOnline || !VisionTargetFound)	//无目标：巡逻搜索
		{
			AimDriveL = AimDriveR = Obstacle ? 0 : PATROL_DUTY;
			Pump_SetDuty(0);
			State = AIM_PATROL;
		}
		else
		{
			int16_t Dx = VisionX - VISION_CX;	//目标相对画面中心的偏差
			int16_t Dy = VisionY - VISION_CY;

			if (State == AIM_PATROL) State = AIM_TURN;		//发现目标：进入转向对准

			if (State == AIM_TURN)				//阶段1：车体原地转向，对准x
			{
				if (Obstacle)					//近障安全停车（安全优先于对准）
				{
					AimDriveL = AimDriveR = 0;
				}
				else if (Dx > AIM_TOL_X)		//目标在画面右侧：原地右转
				{
					AimDriveL =  TURN_DUTY;
					AimDriveR = -TURN_DUTY;
				}
				else if (Dx < -AIM_TOL_X)		//目标在画面左侧：原地左转
				{
					AimDriveL = -TURN_DUTY;
					AimDriveR =  TURN_DUTY;
				}
				else							//x对准：停车，进入舵机微调
				{
					AimDriveL = AimDriveR = 0;
					State = AIM_TRACK;
				}
			}

			if (State == AIM_TRACK)				//阶段2：停车，舵机微调x/y，对准则喷水
			{
				AimDriveL = AimDriveR = 0;		//保持停车

				if (Dx > AIM_TOL_X || Dx < -AIM_TOL_X)	//水平舵机消除x误差
				{
					Servo_SetAngle1(Aim_ClampAngle(
						(int16_t)Servo_GetAngle1() - AIM_YAW_SIGN * Aim_Step(Dx)));
				}
				if (Dy > AIM_TOL_Y || Dy < -AIM_TOL_Y)	//俯仰舵机消除y误差
				{
					Servo_SetAngle2(Aim_ClampAngle(
						(int16_t)Servo_GetAngle2() - AIM_PITCH_SIGN * Aim_Step(Dy)));
				}

				/*对准判定：x、y误差都在±10像素内 -> 喷水，否则停*/
				if (Dx >= -AIM_TOL_X && Dx <= AIM_TOL_X
					&& Dy >= -AIM_TOL_Y && Dy <= AIM_TOL_Y)
				{
					Pump_SetDuty(SPRAY_DUTY);
				}
				else
				{
					Pump_SetDuty(0);
				}

				/*x偏差过大（舵机调不回来）：退回车体转向*/
				if (Dx > 3 * AIM_TOL_X || Dx < -3 * AIM_TOL_X) State = AIM_TURN;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(20));		//50Hz控制周期
	}
}
