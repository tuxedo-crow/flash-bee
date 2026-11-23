// Flash Bee travel case (Prusa Core One+, semi-rigid flexible PETG "Flex-G")
//
// Clamshell for the assembled handheld (about 51.5 x 96.8 x 20.7 mm; the thickness is an
// estimate, so the cavity is generous). Printed open and flat, no supports: two equal trays side
// by side, open side up, outer faces on the bed. They are joined only by a print-in-place knuckle
// hinge along the back walls: five knuckles, the two ends and the middle on the base, the two
// between on the lid, 0.4 mm apart. Each lid knuckle has a 45 degree cone on both end faces that
// sits in a matching socket in the neighbouring base knuckle, so the two halves are interlocked
// but never touch (two separate solids, no pin). The gaps (cone_gap 0.35 normal to the cone,
// kn_gap 0.4, cone_tip_gap 0.4) are PROVISIONAL for Flex-G: unmeasured. Open the case gently the
// first time to break any stringing in the gaps. The case closes with a magnetic clasp: a small
// boss on each front wall, face to face when the lid is shut, each with a well for one 10 x 5 x 3 mm
// block magnet. Each magnet is pressed into its well with a drop of CA glue, large face up and
// flush with the rim plane; check the polarity before gluing the second one (they must attract
// with the lid shut). The lid boss is 2 mm longer: a finger ledge for opening. A vent hole under
// each well lets you push a magnet out. The well clearance (0.25 per side, core1 square_press) is
// PROVISIONAL for Flex-G: unmeasured; the magnet is glued, so a loose fit is tolerable.
//
// Datum: Z = 0 is the bed (outer faces of both trays). The hinge axis runs along Y at X = 0,
// Z = h (the rim plane). Y is the long axis, Y = 0 the centre of the case. Base tray at -X,
// lid tray at +X; their back-wall outer faces sit at X = -hg and X = +hg.
// Base front wall (boss) = base -X wall; lid front wall (boss) = lid +X wall, away from the hinge.
// Boss profiles are in (u, z): u outward from the wall face.
//
// part = "case" | "fit_test" | "closed" | "swing" | "collide"
//   case      everything, as printed (open_angle 180)
//   fit_test  the case cut to a slab (one base knuckle, one lid knuckle with the cone interface, and both bosses with their magnet wells)
//   closed    preview only: lid folded onto the base
//   swing     preview: lid at open_angle (0 = closed, 180 = as printed)
//   collide   the volume shared by base and lid at open_angle (must be empty)

part = "case";
open_angle = 0;      // swing and collide only: 0 = closed, 180 = as printed

$fn = $preview ? 48 : 96;
eps = 0.01;

// ---- Printer (printers.toml, core1) --------------------------------------------------
lw         = 0.45;   // line width
lh         = 0.20;   // layer height
bed        = [250, 220, 270];

// ---- Cavity ----------------------------------------------------------------------------
cav_w      = 54.6;   // cavity width (X)
cav_l      = 99.9;   // cavity length (Y)
cav_r      = 4;      // cavity corner radius
dep_b      = 12.0;   // base cavity depth
dep_l      = 12.0;   // lid cavity depth (equal halves: both rims are coplanar as printed)

// ---- Walls -----------------------------------------------------------------------------
wall       = 2.25;   // 5 lines
floor_t    = 1.2;    // floor
out_r      = cav_r + wall;      // outer corner radius
out_w      = cav_w + 2 * wall;  // outer tray width
out_l      = cav_l + 2 * wall;  // outer tray length
h          = floor_t + dep_b;   // tray height = rim plane = hinge axis height
hb         = h;                 // base height
hl         = floor_t + dep_l;   // lid height

// ---- Hinge -----------------------------------------------------------------------------
hg         = 4.0;    // back-wall outer face to the hinge axis, each side
kn_r       = 3.5;    // barrel radius
kn_n       = 5;      // knuckles; 1, 3, 5 on the base, 2, 4 on the lid
kn_l       = 15.6;   // knuckle length (Y)
kn_gap     = 0.4;    // gap between neighbours (Y)
kn_low     = 4.0;    // height where the arm leaves the wall
kn_ov      = 0.5;    // arm strip overlap into the wall
kn_clip    = 0.05;   // clearance above the rim plane to the other half's wall
cone_r     = 2.2;    // male cone base radius on the lid knuckle face
cone_tip_r = 0.5;    // male cone truncated tip radius
cone_gap   = 0.35;   // gap between male and socket, normal to the cone surface
cone_tip_gap = 0.4;  // axial gap between the male tip and the socket end
cone_h     = cone_r - cone_tip_r;           // male height (45 degree half-angle)
// socket: same axis and 45 degrees, surface r + z = cone_r + cone_gap * sqrt(2) (z from the lid face)
sock_mouth = cone_r + cone_gap * sqrt(2) - kn_gap;       // radius on the base knuckle face
sock_d     = cone_h + cone_tip_gap - kn_gap;             // socket depth into the base knuckle
sock_end   = sock_mouth - sock_d;                        // radius at the socket end
kn_tot     = kn_n * kn_l + (kn_n - 1) * kn_gap;

// ---- Locating lip (base) and rebate (lid) ------------------------------------------------
lip_h      = 1.6;    // lip height above the base rim
lip_t      = 0.9;    // lip thickness, inner face flush with the cavity wall
lip_c      = 0.35;   // rebate clearance on the outside of the lip
reb_h      = lip_h + 0.4;        // rebate height in the lid rim
reb_d      = lip_t + lip_c;      // rebate radial depth from the cavity wall

// ---- Magnetic clasp: bosses on the front walls, top faces in the rim plane z = h --------
mag_l      = 10.0;   // block magnet, along Y
mag_w      = 5.0;    // along X
mag_t      = 3.0;    // thickness (Z)
mag_c      = 0.25;   // clearance per side (core1 square_press; provisional for Flex-G, magnet is glued)
well_l     = mag_l + 2 * mag_c;   // well, Y
well_w     = mag_w + 2 * mag_c;   // well, X
well_dep   = 3.0;    // well depth (15 layers), magnet flush with the rim plane
well_gap   = 0.2;    // well inner edge outside the wall face (the well does not cut the wall)
boss_skin  = 0.9;    // boss beyond the well on the outer side
boss_end   = 1.35;   // boss beyond the well at each Y end
boss_floor = 1.0;    // material under the well
boss_ov    = 0.5;    // overlap into the wall
boss_cr    = 1.5;    // plan corner radius, outer corners
boss_p_b   = well_gap + well_w + boss_skin;   // base boss protrusion
boss_p_l   = boss_p_b + 2.0;                  // lid boss protrusion (finger ledge)
boss_len   = well_l + 2 * boss_end;           // boss length (Y)
well_cx    = well_gap + well_w / 2;           // well centre, outward from the wall face
vent_d     = 2.0;    // vent / push-out hole on the well centre
// (the well has square corners: a corner relief would cut into the wall at the inner edge)

// ---- Fit test slab (Y range, from -9.5 to just after knuckle 4) -------
ft_k0      = 2;      // first knuckle in the fit test (knuckle 3, base)
ft_k1      = 3;      // last knuckle in the fit test (knuckle 4, lid)
ft_y0      = -9.5;
ft_y1      = 24.0;

// ---- Derived and checks ----------------------------------------------------------------
x_off      = hg + out_w / 2;   // tray centre offset from the hinge axis
function mult(v, u) = abs(v / u - round(v / u)) < 1e-6;
// underside angle of the knuckle arm from horizontal: line to the circle centre minus tangent offset
kn_d       = norm([hg, h - kn_low]);
kn_ang     = atan2(h - kn_low, hg) - asin(kn_r / kn_d);

assert(mult(wall, lw), "wall is not a multiple of the line width");
assert(mult(floor_t, lh), "floor_t is not a multiple of the layer height");
assert(mult(hb, lh), "hb is not a multiple of the layer height");
assert(mult(hl, lh), "hl is not a multiple of the layer height");
assert(mult(lip_h, lh), "lip_h is not a multiple of the layer height");
assert(abs(hb - hl) < 1e-6, "halves are not equal: rims are not coplanar");
assert(wall - reb_d >= 0.9 - 1e-6, "lid wall left at the rebate is under 0.9 mm");
assert(lip_c > 0 && reb_h > lip_h, "lip does not fit the rebate");
function boss_under(P, x) = max(0, h - well_dep - boss_floor - P + x);   // underside height at x (45 degrees)
function boss_mat(P) = (h - well_dep) - boss_under(P, well_gap + well_w);   // material under the well's outer edge
assert(mult(well_dep, lh), "well_dep must be a multiple of the layer height");
assert(well_gap > 0, "the well touches the wall face");
assert(abs(boss_p_b - (well_gap + well_w + boss_skin)) < 1e-6 && abs(boss_len - (well_l + 2 * boss_end)) < 1e-6, "boss skins wrong");
assert(boss_mat(boss_p_b) >= boss_floor - 1e-6 && boss_mat(boss_p_l) >= boss_floor - 1e-6, "the underside cuts into the well floor");
assert(h - well_dep - boss_floor - boss_p_b > 0.4 - 1e-6 || h - well_dep - boss_floor - boss_p_b <= 0, "wedge meets the wall within 0.4 mm of the bed");
assert(h - well_dep - boss_floor - boss_p_l > 0.4 - 1e-6 || h - well_dep - boss_floor - boss_p_l <= 0, "wedge meets the wall within 0.4 mm of the bed");
assert(well_dep >= mag_t - 1e-6, "well shallower than the magnet");
assert(vent_d < well_w - 1.5, "vent hole too big");
assert(kn_ang >= 45, "knuckle arm underside is flatter than 45 degrees: lower kn_low");
assert(kn_tot <= out_l - 2 * out_r, "knuckles run into the tray corners");
assert(hg - kn_r >= 0.4, "barrel too close to the other half's back wall");
assert(kn_r - sock_mouth >= 0.9 - 1e-6, "wall around the socket mouth is under 0.9 mm");
assert(cone_gap >= 0.3 && ((sock_mouth + kn_gap) - cone_r) / sqrt(2) >= 0.3 - 1e-6, "cone normal gap under 0.3 mm");
assert(cone_tip_gap >= 0.4 - 1e-6 && sock_end > 0, "cone tip gap under 0.4 mm or socket end closed");
assert(kn_low > floor_t && kn_low < h, "kn_low outside the wall");

echo(str("outer size as printed: ", 2 * (hg + out_w) + boss_p_b + boss_p_l, " x ", out_l, " x ", h + kn_r,
         " (incl. bosses; trays ", out_w, " x ", out_l, " x ", hb, ")"));
echo(str("closed outer size: ", hg + out_w + boss_p_l + kn_r, " x ", out_l, " x ", hb + hl, " (incl. lid boss and hinge barrel)"));
echo(str("magnet face gap when closed: ", 2 * (well_dep - mag_t), " mm; well ", well_l, " x ", well_w, "; boss ", boss_len, " x ", boss_p_b, " / ", boss_p_l,
         "; material under the well's outer edge: base ", boss_mat(boss_p_b), ", lid ", boss_mat(boss_p_l),
         "; beside the well: ", boss_skin, " outer, ", boss_end, " at the ends"));
echo(str("cone normal gap ", ((sock_mouth + kn_gap) - cone_r) / sqrt(2), ", tip gap ", cone_tip_gap,
         ", socket mouth r ", sock_mouth, " end r ", sock_end, " depth ", sock_d, ", wall at mouth ", kn_r - sock_mouth));
echo(str("closed cavity depth: ", dep_b + dep_l, ", knuckle arm underside ", kn_ang, " deg from horizontal"));

// ---- Helpers ---------------------------------------------------------------------------
module rrect(w, l, r, hh) {
    linear_extrude(hh)
        offset(r) square([w - 2 * r, l - 2 * r], center = true);
}

// Tray with its centre at the origin, bed at Z = 0.
module tray(hh, dep) {
    difference() {
        rrect(out_w, out_l, out_r, hh);
        translate([0, 0, floor_t]) rrect(cav_w, cav_l, cav_r, dep + eps);
    }
}

// Knuckle i (0-based), side -1 = base, +1 = lid. Profile in XZ, extruded along Y.
// Knuckles lo..hi exist (fit_test uses a subset); cones and sockets only face a neighbour that exists.
module knuckle(i, side, lo, hi) {
    y0 = -kn_tot / 2 + i * (kn_l + kn_gap);
    translate([0, y0 + kn_l, 0]) rotate([90, 0, 0]) linear_extrude(kn_l)
        difference() {
            hull() {
                translate([0, h]) circle(r = kn_r, $fn = 96);
                // strip on the tray's own back wall, overlapping kn_ov into it
                translate([side < 0 ? -hg - kn_ov : hg, kn_low]) square([kn_ov, h - kn_low]);
            }
            // nothing above the rim plane past the other half's wall face
            translate([side < 0 ? -20 : hg - kn_clip, h]) square([20 + (side < 0 ? -hg + kn_clip : 0), 20]);
        }
    for (dir = [-1, 1]) if (i + dir >= lo && i + dir <= hi) {
        yf = dir < 0 ? y0 : y0 + kn_l;      // this knuckle's face toward the neighbour
        if (side > 0)
            // male cone, pointing into the gap and the neighbour's socket; 0.3 mm run-in under the face
            translate([0, yf, h]) rotate([dir > 0 ? -90 : 90, 0, 0]) translate([0, 0, -0.3])
                cylinder(h = cone_h + 0.3, r1 = cone_r + 0.3, r2 = cone_tip_r);
    }
}

// Sockets are cut from the base knuckles after they are built.
module socket(i, lo, hi) {
    y0 = -kn_tot / 2 + i * (kn_l + kn_gap);
    for (dir = [-1, 1]) if (i + dir >= lo && i + dir <= hi) {
        yf = dir < 0 ? y0 : y0 + kn_l;
        translate([0, yf, h]) rotate([dir > 0 ? 90 : -90, 0, 0]) translate([0, 0, -0.3])
            cylinder(h = sock_d + 0.3, r1 = sock_mouth + 0.3, r2 = sock_end);
    }
}

module knuckles(side, lo = 0, hi = kn_n - 1) {
    for (i = [lo : hi]) if ((i % 2 == 0) == (side < 0)) {
        if (side < 0)
            difference() { knuckle(i, side, lo, hi); socket(i, lo, hi); }
        else knuckle(i, side, lo, hi);
    }
}

// Boss with its 45 degree buttress, outline (u, z), extruded over boss_len and rounded in plan.
module boss(P) {
    bt = well_dep + boss_floor;
    zw = h - bt - P;                      // height where the 45 degree underside meets the wall
    under = zw > 0 ? [[0, zw], [-boss_ov, zw]] : [[P - (h - bt), 0], [-boss_ov, 0]];
    intersection() {
        rotate([90, 0, 0]) linear_extrude(boss_len, center = true)
            polygon(concat([[-boss_ov, h], [P, h], [P, h - bt]], under));
        translate([0, 0, -1]) linear_extrude(h + 2)
            hull() {
                translate([-boss_ov - 1, -boss_len / 2]) square([1, boss_len]);
                for (v = [-1, 1]) translate([P - boss_cr, v * (boss_len / 2 - boss_cr)]) circle(r = boss_cr);
            }
    }
}

// Well and vent cutter, z = 0 at the rim plane; x outward from the wall face.
module well_cut() {
    translate([well_gap, -well_l / 2, -well_dep]) cube([well_w, well_l, well_dep + 1]);
    translate([well_cx, 0, -h - 1]) cylinder(d = vent_d, h = h + 1 - well_dep + eps, $fn = 48);
}

module base_all(lo = 0, hi = kn_n - 1) {
    translate([-x_off, 0, 0]) {
        tray(hb, dep_b);
        // locating lip
        translate([0, 0, hb - eps])
            difference() {
                rrect(cav_w + 2 * lip_t, cav_l + 2 * lip_t, cav_r + lip_t, lip_h + eps);
                translate([0, 0, -eps]) rrect(cav_w, cav_l, cav_r, lip_h + 3 * eps);
            }
        // boss on the front (-X) wall, mirrored to -X
        translate([-out_w / 2, 0, 0]) mirror([1, 0, 0])
            difference() { boss(boss_p_b); translate([0, 0, h]) well_cut(); }
    }
    knuckles(-1, lo, hi);
}

module lid_all(lo = 0, hi = kn_n - 1) {
    translate([x_off, 0, 0]) {
        difference() {
            tray(hl, dep_l);
            // rebate for the lip
            translate([0, 0, hl - reb_h])
                rrect(cav_w + 2 * reb_d, cav_l + 2 * reb_d, cav_r + reb_d, reb_h + eps);
        }
        // boss on the front (+X) wall
        translate([out_w / 2, 0, 0])
            difference() { boss(boss_p_l); translate([0, 0, h]) well_cut(); }
    }
    knuckles(1, lo, hi);
}

// Lid turned about the hinge axis (X = 0, Z = h) to open_angle: 180 as printed, 0 closed.
module lid_swing(a, lo = 0, hi = kn_n - 1) {
    translate([0, 0, h]) rotate([0, -(180 - a), 0]) translate([0, 0, -h]) lid_all(lo, hi);
}

if (part == "case") { base_all(); lid_all(); }
else if (part == "fit_test")
    intersection() {
        union() { base_all(ft_k0, ft_k1); lid_all(ft_k0, ft_k1); }
        translate([-200, ft_y0, -1]) cube([400, ft_y1 - ft_y0, 100]);
    }
else if (part == "closed") { base_all(); lid_swing(0); }
else if (part == "swing") { base_all(); lid_swing(open_angle); }
else if (part == "collide")
    intersection() { base_all(); lid_swing(open_angle); }
