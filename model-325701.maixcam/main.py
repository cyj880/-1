
from maix import camera, display, image, nn, app, uart
import struct, os

# 害虫检测 + 坐标发送给STM32F407小车 (USART3: STM32 PD8=TX, PD9=RX)
# 接线: MaixCAM TX(A16) -> STM32 PD9(RX)
#        MaixCAM RX(A17) -> STM32 PD8(TX)  (可选,本程序只发不收)
#        GND 共地
#
# 通信协议: 115200-8N1, 每帧13字节(小端):
#   AA 55 | x(int16) y(int16) w(uint16) h(uint16) count(uint16) | sum校验
#   x,y = 主目标(最大目标)中心像素坐标; count = 本帧检测到的目标总数
#   无目标时发送 count=0 的帧,STM32据此判断"当前无目标"

model_path = "model_325701.mud"
if not os.path.exists(model_path):
    model_path = "/root/models/haichon/model_325701.mud"
detector = nn.YOLOv5(model=model_path)

cam = camera.Camera(detector.input_width(), detector.input_height(), detector.input_format())
dis = display.Display()

ser = uart("/dev/ttyS0", 115200)     # MaixCAM板载串口,连STM32

def make_frame(cx, cy, w, h, cnt):
    payload = struct.pack("<hhHHH", cx, cy, w, h, cnt)
    return b'\xAA\x55' + payload + bytes([sum(payload) & 0xFF])

while not app.need_exit():
    img = cam.read()
    objs = detector.detect(img, conf_th=0.5, iou_th=0.45)

    if len(objs) > 0:
        best = max(objs, key=lambda o: o.w * o.h)      # 取最大目标作为主目标(对靶用)
        cx = best.x + best.w // 2
        cy = best.y + best.h // 2
        ser.write(make_frame(cx, cy, best.w, best.h, len(objs)))
    else:
        ser.write(make_frame(0, 0, 0, 0, 0))           # 无目标帧

    for obj in objs:
        img.draw_rect(obj.x, obj.y, obj.w, obj.h, color=image.COLOR_RED)
        msg = f'{detector.labels[obj.class_id]}: {obj.score:.2f}'
        img.draw_string(obj.x, obj.y, msg, color=image.COLOR_RED)
    dis.show(img)
