# -*- coding: utf-8 -*-
"""
render_ui_wireframe.py — 新 UI 结构线框（导航式布局，替代原 5 个 Tab）

主视图里的车位图仍然由 out/lot-a 的真实 layout.json + active.json 驱动。
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from render_mockup import (  # noqa: E402
    ACCENT, AMBER, ASSETS, BG, CARD, CARD_LINE, FREE, FREE_LINE, MUTED, TEXT, TYPE_COLOR,
    TYPE_LABEL, draw_lot, font, load_lot, lot_extent,
)

W, H = 1400, 830
NAV = (21, 25, 33)
ROW = (30, 35, 45)


def main():
    cfg_a, lay_a, act_a, wai_a = load_lot("lot-a")
    occ_a = {e["slot"]: e for e in act_a["entries"]}
    free_a = cfg_a["capacity"] - len(occ_a)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # ---------------- 窗口 ----------------
    d.rounded_rectangle([36, 26, 1364, 804], radius=14, fill=(17, 20, 26), outline=(48, 56, 70), width=1)
    d.rounded_rectangle([36, 26, 1364, 62], radius=14, fill=(22, 26, 34), outline=(48, 56, 70), width=1)
    d.text((56, 36), "停车场管理系统", font=font(13, True), fill=TEXT)
    d.text((1180, 36), "—   □   ✕", font=font(13), fill=MUTED)

    # ---------------- 左侧导航 ----------------
    d.rectangle([36, 62, 246, 804], fill=NAV)
    d.text((58, 92), "P", font=font(26, True), fill=ACCENT)
    d.text((88, 96), "PARKING", font=font(13, True), fill=TEXT)
    d.text((88, 114), "停车场后台", font=font(10), fill=MUTED)

    nav = [("总览", True), ("停车操作", False), ("车辆查询", False), ("统计报表", False), ("系统设置", False)]
    y = 160
    for name, active in nav:
        if active:
            d.rounded_rectangle([46, y, 236, y + 40], radius=8, fill=(33, 41, 58))
            d.rounded_rectangle([46, y + 8, 50, y + 32], radius=2, fill=ACCENT)
        d.text((68, y + 12), name, font=font(13, active), fill=TEXT if active else MUTED)
        y += 48

    d.line([58, 720, 226, 720], fill=(44, 52, 67), width=1)
    d.text((58, 734), "数据状态", font=font(10), fill=MUTED)
    d.ellipse([58, 754, 66, 762], fill=(53, 192, 138))
    d.text((74, 750), "已自动保存", font=font(11), fill=TEXT)
    d.text((74, 768), "journal 3 条待压缩", font=font(10), fill=MUTED)

    # ---------------- 顶部工具条 ----------------
    d.text((268, 80), "总览", font=font(18, True), fill=TEXT)
    d.text((268, 106), "实时车位、等待队列与今日流水", font=font(11), fill=MUTED)

    d.rounded_rectangle([1010, 80, 1170, 116], radius=8, fill=ACCENT)
    d.text((1040, 90), "＋ 车辆入库", font=font(13, True), fill=(255, 255, 255))
    d.rounded_rectangle([1182, 80, 1342, 116], radius=8, fill=(30, 36, 47), outline=(56, 64, 80), width=1)
    d.text((1212, 90), "－ 车辆出场", font=font(13), fill=TEXT)

    # ---------------- KPI ----------------
    kx, ky, kw, kh, gap = 268, 136, 168, 62, 10
    chips = [
        ("总车位", str(cfg_a["capacity"]), TEXT),
        ("场内", str(len(occ_a)), (111, 168, 255)),
        ("空余", str(free_a), (53, 192, 138)),
        ("今日收入", "¥ %.2f" % (act_a["revenueCents"] / 100.0), (240, 169, 46)),
    ]
    for i, (label, value, color) in enumerate(chips):
        x = kx + i * (kw + gap)
        d.rounded_rectangle([x, ky, x + kw, ky + kh], radius=9, fill=CARD, outline=CARD_LINE, width=1)
        d.text((x + 14, ky + 9), label, font=font(11), fill=MUTED)
        d.text((x + 14, ky + 27), value, font=font(19, True), fill=color)

    # ---------------- 2D 车位视图 ----------------
    vx, vy, vw, vh = 268, 214, 700, 348
    d.rounded_rectangle([vx, vy, vx + vw, vy + vh], radius=10, fill=CARD, outline=CARD_LINE, width=1)
    d.text((vx + 18, vy + 14), "2D 车位视图", font=font(14, True), fill=TEXT)
    d.text((vx + 130, vy + 17), "单击车位查看详情 · 滚轮缩放 · 拖拽平移", font=font(10), fill=MUTED)

    w_a, h_a = lot_extent(lay_a)
    sc = min((vw - 40) / w_a, (vh - 66) / h_a)
    ox = vx + (vw - w_a * sc) / 2
    draw_lot(d, ox, vy + 44, lay_a, occ_a, sc)

    # ---------------- 最近出场流水 ----------------
    tx, ty, tw, th = 268, 578, 700, 206
    d.rounded_rectangle([tx, ty, tx + tw, ty + th], radius=10, fill=CARD, outline=CARD_LINE, width=1)
    d.text((tx + 18, ty + 14), "最近出场", font=font(14, True), fill=TEXT)
    cols = [("车牌", tx + 18), ("车型", tx + 190), ("车位", tx + 300), ("时长", tx + 386), ("金额", tx + 512)]
    for name, x in cols:
        d.text((x, ty + 46), name, font=font(10), fill=MUTED)
    d.line([tx + 18, ty + 64, tx + tw - 18, ty + 64], fill=(44, 52, 67), width=1)
    rows = [("鄂A00012", "小汽车", "P012", "2h 15m", "¥ 15.00"),
            ("京A00341", "中卡", "P047", "0h 42m", "¥ 5.00"),
            ("沪A00777", "大卡", "P003", "5h 08m", "¥ 30.00")]
    ry = ty + 76
    for i, (p, t, s, dur, fee) in enumerate(rows):
        if i == 1:
            d.rounded_rectangle([tx + 10, ry - 6, tx + tw - 10, ry + 30], radius=6, fill=ROW)
        d.text((tx + 18, ry), p, font=font(11), fill=TEXT)
        d.text((tx + 190, ry), t, font=font(11), fill=TEXT)
        d.text((tx + 300, ry), s, font=font(11), fill=MUTED)
        d.text((tx + 386, ry), dur, font=font(11), fill=MUTED)
        d.text((tx + 512, ry), fee, font=font(11, True), fill=(53, 192, 138))
        ry += 42

    # ---------------- 右侧：队列 + 结构说明 ----------------
    qx0, qy0, qw = 988, 214, 356
    d.rounded_rectangle([qx0, qy0, qx0 + qw, qy0 + 170], radius=10, fill=CARD, outline=CARD_LINE, width=1)
    d.text((qx0 + 18, qy0 + 14), "等待队列", font=font(14, True), fill=AMBER)
    d.text((qx0 + 96, qy0 + 17), "FIFO · 出场自动补位", font=font(10), fill=MUTED)
    if wai_a["entries"]:
        for i, w in enumerate(wai_a["entries"][:4]):
            col = TYPE_COLOR.get(w["type"], ACCENT)
            yy = qy0 + 44 + i * 30
            d.rounded_rectangle([qx0 + 18, yy, qx0 + 24, yy + 22], radius=2, fill=col)
            d.text((qx0 + 34, yy + 3), w["plate"], font=font(11), fill=TEXT)
            d.text((qx0 + 200, yy + 3), TYPE_LABEL.get(w["type"], "?"), font=font(10), fill=MUTED)
    else:
        d.text((qx0 + 18, qy0 + 56), "队列为空（有车位时不会排队）", font=font(11), fill=MUTED)

    d.rounded_rectangle([qx0, 402, qx0 + qw, 784], radius=10, fill=CARD, outline=CARD_LINE, width=1)
    d.text((qx0 + 18, 416), "结构变化", font=font(14, True), fill=TEXT)
    notes = [
        ("1", "左侧导航 + QStackedWidget", "替代原来 5 个 Tab 平铺"),
        ("2", "2D 视图常驻总览页", "取代独立「图形视图」标签页"),
        ("3", "入库/出场改为对话框", "主界面不再堆表单与提示框"),
        ("4", "表格用 Model/View", "增量刷新，不再每 5 秒清空重建"),
        ("5", "状态栏 + 数据保存指示", "journal 待压缩量可见"),
    ]
    ny = 452
    for num, title, sub in notes:
        d.ellipse([qx0 + 18, ny + 2, qx0 + 36, ny + 20], fill=(33, 41, 58), outline=ACCENT, width=1)
        d.text((qx0 + 24, ny + 5), num, font=font(10, True), fill=ACCENT)
        d.text((qx0 + 46, ny), title, font=font(12, True), fill=TEXT)
        d.text((qx0 + 46, ny + 20), sub, font=font(10), fill=MUTED)
        ny += 64

    os.makedirs(ASSETS, exist_ok=True)
    out = os.path.join(ASSETS, "mockup_ui.png")
    img.save(out, "PNG")
    print("written:", out, img.size)


if __name__ == "__main__":
    main()
