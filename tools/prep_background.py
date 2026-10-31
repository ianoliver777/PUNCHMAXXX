"""Builds assets/background.png from the UI concept by erasing the static parts
that the plugin draws live (knob pointers + value arcs, focus readout, the
preset name). Prints measured knob geometry for Layout.h."""
import sys, math
import numpy as np, cv2

src, dst = sys.argv[1], sys.argv[2]
im = cv2.imread(src)  # BGR
h, w = im.shape[:2]
b, g, r = [im[..., i].astype(int) for i in range(3)]
red = (r > 105) & (r - g > 55) & (r - b > 45)
redLoose = (r > 60) & (r - g > 28) & (r - b > 22)

# name: (cx, cy, capR, bezelR)
knobs = {   # 1536 x 1024 art: (cx, cy, capR, bezelR)
    "transient": (400.5, 580.2, 68, 90), "punch": (667.4, 580.5, 68, 90), "body": (929.3, 580.0, 68, 90),
    "drive": (1172.2, 352.5, 48, 59), "saturation": (1172.7, 558.7, 48, 59), "output": (1385.4, 621.5, 40, 50),
    "focus": (509.9, 867.5, 41, 50), "low": (748.1, 846.5, 40, 49), "high": (922.6, 846.5, 40, 49),
    "width": (1136.6, 849.5, 42, 55), "mix": (1367.4, 849.9, 42, 55),
}
ARC_MAX = {'transient': 95, 'punch': 95, 'body': 95, 'drive': 63, 'saturation': 64,
           'output': 50, 'focus': 60, 'low': 48, 'high': 48, 'width': 57, 'mix': 57}
yy, xx = np.mgrid[0:h, 0:w]
mask = np.zeros((h, w), np.uint8)
for name, (cx, cy, capR, bz) in knobs.items():
    d = np.hypot(xx - cx, yy - cy)
    # mirror the clean left half of the bezel ring onto the right half (kills the baked value arc)
    outer = ARC_MAX[name] + 1.5
    ring = (d > capR - 1) & (d < outer) & (xx > cx) & (yy < cy + 0.5 * bz) & ~((np.abs(xx - cx) < 10) & (yy < cy - capR - 3))
    ys, xs = np.where(ring)
    srcx = np.clip(np.round(2 * cx - xs).astype(int), 0, w - 1)
    im[ys, xs] = im[ys, srcx]
    b, g, r = [im[..., i].astype(int) for i in range(3)]
    redLoose = (r > 60) & (r - g > 28) & (r - b > 22)
    m = (redLoose | ((r > 40) & (r > g * 1.5) & (r > b * 1.5))) & (d < outer + 2) & (d > capR - 2)
    m = m | (redLoose & (d < capR))
    strip = (np.abs(xx - cx) < 4) & (yy > cy - capR - 2) & (yy < cy - 4)
    m = m | strip
    mask |= m.astype(np.uint8)

def box(x0, y0, x1, y1):
    mask[y0:y1, x0:x1] = 1

box(397, 882, 439, 910)   # "180" label (replaced by 50 live)
box(486, 780, 534, 809)   # "200" focus readout (drawn live)
box(705, 189, 905, 218)   # preset name text (drawn live)
box(502, 804, 518, 823)   # tip of the baked FOCUS pointer

mask = cv2.dilate(mask * 255, np.ones((5, 5), np.uint8))
flatBoxes = [(396, 881, 440, 911), (485, 779, 535, 810)]
for x0, y0, x1, y1 in flatBoxes:   # flat-fill first so the inpaint never samples the old text
    ring = np.concatenate([im[y0 - 3:y0, x0:x1].reshape(-1, 3), im[y1 + 14:y1 + 17, x0:x1].reshape(-1, 3)])
    im[y0:y1, x0:x1] = np.median(ring, axis=0)
    mask[y0:y1, x0:x1] = 0
clean = cv2.inpaint(im, mask, 4, cv2.INPAINT_TELEA)
for x0, y0, x1, y1 in flatBoxes:
    ring = np.concatenate([clean[y0 - 3:y0, x0:x1].reshape(-1, 3), clean[y1:y1 + 3, x0:x1].reshape(-1, 3)])
    clean[y0:y1, x0:x1] = np.median(ring, axis=0)
cv2.imwrite(dst, clean)
print("wrote", dst)
