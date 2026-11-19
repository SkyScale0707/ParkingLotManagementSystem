# -*- coding: utf-8 -*-
"""
render_mockup.py — 用原型写出的真实 JSON 数据渲染 2D 视图效果图。

数据全部来自 out/lot-a、lot-b 的 gen-N/ 快照：
  config.json / layout.json / active.json / waiting.json
不构造任何假数据，几何完全由 layout.json 驱动。
"""
import json
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "out")                    # 原型程序以仓库根为工作目录
ASSETS = os.path.join(ROOT, "docs", "assets")      # 需提交的效果图

BG = (15, 17, 21)
CARD = (25, 29, 38)
CARD_LINE = (40, 47, 60)
TEXT = (228, 232, 240)
MUTED = (138, 148, 166)
FREE = (26, 30, 39)
FREE_LINE = (44, 52, 67)
ACCENT = (79, 140, 255)
AMBER = (240, 169, 46)

TYPE_COLOR = {
    "car_small": (79, 140, 255),
    "truck_small": (53, 192, 138),
    "truck_medium": (240, 169, 46),
    "truck_large": (226, 86, 79),
}
TYPE_LABEL = {
    "car_small": "小汽车",
    "truck_small": "小卡",
    "truck_medium": "中卡",
    "truck_large": "大卡",
}


def font(size, bold=False):
    for c in ([r"C:\Windows\Fonts\msyhbd.ttc"] if bold else []) + [
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
    ]:
        if os.path.exists(c):
            try:
                return ImageFont.truetype(c, size)
            except Exception:
                continue
    return ImageFont.load_default()


def load_lot(name):
    d = os.path.join(OUT, name)
    with open(os.path.join(d, "CURRENT"), encoding="utf-8") as f:
        gen = f.read().strip()
    g = os.path.join(d, "gen-" + gen)
    out = []
    for fn in ("config.json", "layout.json", "active.json", "waiting.json"):
        with open(os.path.join(g, fn), encoding="utf-8") as f:
            out.append(json.load(f))
    return out


def lot_extent(lay):
    w = lay["margin"] * 2 + lay["cols"] * (lay["slotW"] + lay["gapX"]) - lay["gapX"]
    h = max(s["y"] + s["h"] for s in lay["slots"]) + lay["margin"]
    return w, h


def card(d, box, radius=10):
    d.rounded_rectangle(box, radius=radius, fill=CARD, outline=CARD_LINE, width=1)


def draw_lot(d, ox, oy, lay, occ, scale):
    sw = lay["slotW"] * scale
    sh = lay["slotH"] * scale

    # 每 aisleEvery 行之后的行车道
    for r in range(lay["aisleEvery"] - 1, lay["rows"] - 1, lay["aisleEvery"]):
        idx = min(r * lay["cols"], len(lay["slots"]) - 1)
        y = oy + (lay["slots"][idx]["y"] + lay["slotH"] + lay["gapY"] / 2.0) * scale
        x0 = ox + lay["margin"] * scale
        x1 = ox + (lay["margin"] + lay["cols"] * (lay["slotW"] + lay["gapX"]) - lay["gapX"]) * scale
        xs = x0
        while xs < x1:
            d.line([(xs, y), (min(xs + 8, x1), y)], fill=(56, 64, 80), width=1)
            xs += 15

    for s in lay["slots"]:
        x = ox + s["x"] * scale
        y = oy + s["y"] * scale
        box = [x, y, x + sw, y + sh]
        entry = occ.get(s["i"])
        rad = max(2, int(4 * scale))
        if entry is None:
            d.rounded_rectangle(box, radius=rad, fill=FREE, outline=FREE_LINE, width=1)
            continue
        col = TYPE_COLOR.get(entry["type"], ACCENT)
        d.rounded_rectangle(box, radius=rad, fill=col, outline=col, width=1)
        inset = max(2.0, 4.0 * scale)
        d.rounded_rectangle(
            [x + inset, y + inset, x + sw - inset, y + sh - inset],
            radius=max(1, int(3 * scale)),
            fill=tuple(int(c * 0.58) for c in col),
        )
        # 前挡风
        wy = y + sh * 0.24
        d.line([(x + sw * 0.24, wy), (x + sw * 0.76, wy)],
               fill=tuple(min(255, int(c * 1.15)) for c in col), width=max(1, int(1.6 * scale)))


def kpi_chip(d, x, y, w, h, label, value, color=TEXT):
    d.rounded_rectangle([x, y, x + w, y + h], radius=9, fill=(22, 26, 34), outline=CARD_LINE, width=1)
    d.text((x + 14, y + 9), label, font=font(12), fill=MUTED)
    d.text((x + 14, y + 27), value, font=font(20, True), fill=color)


def main():
    cfg_a, lay_a, act_a, wai_a = load_lot("lot-a")
    cfg_b, lay_b, act_b, wai_b = load_lot("lot-b")

    occ_a = {e["slot"]: e for e in act_a["entries"]}
    occ_b = {e["slot"]: e for e in act_b["entries"]}
    free_a = cfg_a["capacity"] - len(occ_a)

    W, H = 1520, 1000
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # ---------------- 顶部 ----------------
    d.text((40, 30), "停车场管理系统", font=font(28, True), fill=TEXT)
    d.text((40, 70), "2D 视图布局验证 · 几何来自 layout.json，占用状态由 active.json 推导（无第二份真相）",
           font=font(13), fill=MUTED)
    d.rounded_rectangle([W - 252, 32, W - 40, 66], radius=8, fill=(26, 32, 44), outline=CARD_LINE, width=1)
    d.text((W - 240, 42), "QGraphicsView 渲染原型", font=font(12), fill=ACCENT)

    # ---------------- KPI ----------------
    kx, ky, kw, kh, gap = 40, 108, 168, 62, 12
    kpi_chip(d, kx, ky, kw, kh, "总车位", str(cfg_a["capacity"]))
    kpi_chip(d, kx + (kw + gap), ky, kw, kh, "场内", str(len(occ_a)), (111, 168, 255))
    kpi_chip(d, kx + 2 * (kw + gap), ky, kw, kh, "空余", str(free_a), (53, 192, 138))
    kpi_chip(d, kx + 3 * (kw + gap), ky, kw, kh, "排队", str(len(wai_a["entries"])), AMBER)
    kpi_chip(d, kx + 4 * (kw + gap), ky, kw, kh, "累计收入", "¥ %.2f" % (act_a["revenueCents"] / 100.0))

    # ---------------- A 区 ----------------
    ax, ay, aw, ah = 40, 190, 590, 706
    card(d, [ax, ay, ax + aw, ay + ah])
    d.text((ax + 20, ay + 16), "A 区 · 未满场（%d 位，空 %d）" % (cfg_a["capacity"], free_a),
           font=font(15, True), fill=TEXT)
    d.text((ax + 20, ay + 42), "每 2 行一条行车道；车位按行优先连续编号", font=font(11), fill=MUTED)

    w_a, h_a = lot_extent(lay_a)
    scale_a = min((aw - 36) / w_a, (ah - 96) / h_a)
    draw_lot(d, ax + 18, ay + 70, lay_a, occ_a, scale_a)

    # ---------------- B 区（满场 + 队列） ----------------
    bx, by, bw, bh = 650, 190, 830, 706
    card(d, [bx, by, bx + bw, by + bh])
    d.text((bx + 20, by + 16), "B 区 · 满场（%d 位，排队 %d）" % (cfg_b["capacity"], len(wai_b["entries"])),
           font=font(15, True), fill=TEXT)
    d.text((bx + 20, by + 42), "满场后新车进入 FIFO 等待队列；出场时队首自动补位", font=font(11), fill=MUTED)

    w_b, h_b = lot_extent(lay_b)
    scale_b = min((bw - 48) / w_b, (bh - 96 - 120) / h_b)
    draw_lot(d, bx + 24, by + 70, lay_b, occ_b, scale_b)

    # ---------------- 等待队列 ----------------
    qy = by + 70 + h_b * scale_b + 34
    d.text((bx + 24, qy), "等待队列 · FIFO 依次补位", font=font(13, True), fill=AMBER)
    qx, qyy = bx + 24, qy + 26
    for w in wai_b["entries"][:7]:
        col = TYPE_COLOR.get(w["type"], ACCENT)
        d.rounded_rectangle([qx, qyy, qx + 92, qyy + 48], radius=7,
                            fill=tuple(int(c * 0.5) for c in col), outline=col, width=1)
        d.text((qx + 9, qyy + 6), w["plate"], font=font(11, True), fill=(240, 244, 250))
        d.text((qx + 9, qyy + 25), TYPE_LABEL.get(w["type"], "?"), font=font(10), fill=(225, 232, 245))
        qx += 102

    # ---------------- 图例 ----------------
    ly = 928
    d.text((40, ly), "图例", font=font(12, True), fill=MUTED)
    lx = 92
    for name, fill, line in [("空位", FREE, FREE_LINE)] + [(TYPE_LABEL[k], v, v) for k, v in TYPE_COLOR.items()]:
        d.rounded_rectangle([lx, ly + 1, lx + 18, ly + 13], radius=3, fill=fill, outline=line, width=1)
        d.text((lx + 24, ly - 1), name, font=font(11), fill=TEXT)
        lx += 24 + int(d.textlength(name, font=font(11))) + 28

    d.text((W - 520, ly - 1),
           "占用率 %.0f%%   ·   渲染节点数 ≈ %d   ·   QGraphicsView 可承载 1 万+ item" %
           (100.0 * len(occ_a) / cfg_a["capacity"], cfg_a["capacity"] * 2),
           font=font(11), fill=MUTED)

    os.makedirs(ASSETS, exist_ok=True)
    out = os.path.join(ASSETS, "mockup_2d.png")
    img.save(out, "PNG")
    print("written:", out, img.size)


if __name__ == "__main__":
    main()
