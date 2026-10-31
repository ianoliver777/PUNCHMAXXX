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
knobs = {
    "transient": (311.5, 458.5, 61, 79), "punch": (519.5, 458.5, 61, 79), "body": (724, 458.5, 61, 79),
    "drive": (915, 279, 40, 54), "saturation": (915, 435.5, 40, 54), "output": (1080.5, 485.5, 31, 44),
    "focus": (396.5, 672.5, 29, 42), "low": (584.5, 661.5, 31, 42), "high": (719.5, 661.5, 31, 42),
    "width": (887.5, 661.5, 33, 45), "mix": (1067.5, 661.5, 33, 45),
}
ARC_MAX = {'transient': 80, 'punch': 80, 'body': 80, 'drive': 54.5, 'saturation': 53,
           'output': 41.5, 'focus': 43, 'low': 37, 'high': 37, 'width': 48, 'mix': 47}
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

box(312, 688, 346, 711)   # "180" label (replaced by 50 live)
box(380, 610, 418, 633)   # "200" focus readout (drawn live)
box(548, 148, 700, 173)   # preset name text (drawn live)

# flat panel fills for the two FOCUS labels we redraw (inpainting smears the nearby text)
def flat(x0, y0, x1, y1):
    ring = np.concatenate([im[y0 - 3:y0, x0:x1].reshape(-1, 3), im[y1:y1 + 3, x0:x1].reshape(-1, 3)])
    im[y0:y1, x0:x1] = np.median(ring, axis=0)
    mask[y0:y1, x0:x1] = 0

mask = cv2.dilate(mask * 255, np.ones((5, 5), np.uint8))
flatBoxes = [(310, 687, 348, 712), (378, 609, 420, 634)]
for x0, y0, x1, y1 in flatBoxes:
    mask[y0:y1, x0:x1] = 0
clean = cv2.inpaint(im, mask, 4, cv2.INPAINT_TELEA)
for x0, y0, x1, y1 in flatBoxes:
    ring = np.concatenate([clean[y0 - 3:y0, x0:x1].reshape(-1, 3), clean[y1:y1 + 3, x0:x1].reshape(-1, 3)])
    clean[y0:y1, x0:x1] = np.median(ring, axis=0)
cv2.imwrite(dst, clean)
print("wrote", dst)
