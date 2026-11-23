// Flash Bee solderless enclosure (Prusa Core One+, PETG)
//
// No soldering: the AS3935 Grove board keeps its Grove socket and connects with a
// Grove-to-female-jumper cable to a microSD "sniffer" adapter in the Round Display's
// empty microSD slot. The battery plugs into the display's JST 1.25 socket. The display's
// own ON/OFF slide switch is worked by a printed lever (thumb tab on the right side).
// Only the AS3935 I2C solder bridges are still needed.
//
// Datum: display centre = X0 Y0. Z = 0 is the back outer face, Z = case_d the front face.
// Front view: X right, Y up; the XIAO's USB-C points +Y (top edge).
// Display positions come from Seeed's Round Display v1.1 board file
// (Eagle top-layer point (x, y) -> case (-y, -x)).
//
// part = "front" | "back" | "lever" | "fit_test" | "assembly" | "exploded"

part = "exploded";

$fn = $preview ? 48 : 96;
eps = 0.01;

// ---- Printer (printers.toml, core1, PETG; per side) --------------------------------
lw         = 0.45;   // line width
lh         = 0.20;   // layer height
clr_slip   = 0.10;   // round slip
clr_free   = 0.15;   // round free
clr_sq     = 0.30;   // square slip (INFERRED, unmeasured)

// ---- Case ----------------------------------------------------------------------------
case_d     = 30;     // total depth
wall       = 2.25;   // 5 lines
front_t    = 2.0;    // front plate
lip_t      = 1.2;    // front plate over the display glass rim
lid_t      = 2.0;    // back lid
top_y      = 28.5;   // outer top edge
bot_y      = -56;    // outer bottom edge
half_w     = 29;     // outer half width
r_top      = 7;      // outer corner radius, top
r_bot      = 10;     // outer corner radius, bottom
chamfer    = 1.0;    // front edge chamfer (stepped in 0.2 layers)
lid_clr    = 0.15;   // lid outline to wall, per side

// ---- Round Display (Seeed v1.1) -------------------------------------------------------
pcb_dia    = 43.06;
lcd_dia    = 43.0;
lcd_t      = 3.05;   // LCD + touch module (datasheet)
disp_gap   = 0.30;   // tape between LCD and PCB (estimate)
pcb_t      = 1.2;    // estimate
pocket_d   = 43.4;   // display pocket (0.17 per side over 43.06)
window_d   = 34.0;   // view area is 32.9
ring_t     = 1.35;   // locating ring wall, 3 lines
ring_gap   = 30;     // +/- deg, gap at the FPC fold (case angle 0)

z_glass    = case_d - lip_t;                  // 28.8
z_pcbf     = z_glass - lcd_t - disp_gap;      // 25.45
z_pcbb     = z_pcbf - pcb_t;                  // 24.25  display PCB back face
z_fp_in    = case_d - front_t;                // 28.0   front plate inner face

hole_j8    = [-19.431, 0];       // display M2 holes used as locating pins
hole_j6    = [ 15.621, -11.811];
pin_d      = 1.8;                // into 2.2 holes

// ---- XIAO on the display headers ----------------------------------------------------
xiao_usb_z = z_pcbb - 5 - 2.5 - 1.0 - 1.6;    // USB-C centre: socket 5, header 2.5, PCB 1.0
usb_hole   = [13.0, 7.5];        // overmold opening (X, Z)

// ---- Display ON/OFF switch (SW1) and lever --------------------------------------------
knob       = [9.485, -19.188];   // actuator centre (case frame)
knob_u     = [0.891, 0.454];     // slide direction (tangent)
knob_n     = [0.454, -0.891];    // perpendicular, away from centre side used for the pivot
knob_w     = 2.8;                // actuator width along the slide
lever_l1   = 6;                  // pivot -> knob
pivot      = knob + lever_l1 * knob_n;   // [12.21, -24.53]
lev_z0     = 21.0;               // lever bottom
lev_t      = 1.6;                // lever arm thickness (8 layers)
prong_top  = z_pcbb - 0.25;      // 24.0
hub_d      = 6;
pivot_pin  = 2.4;
lev_drop   = 2.8;                // the lever goes in 2.8 low, then up onto the pin
tab        = [2.0, 4.0, 2.4];    // thumb tab outside the wall (X, Y, Z)
slot_h     = 8.0;                // wall slot along Y (knob travel up to ~2 mm)

// ---- microSD sniffer (SparkFun TOL-09419 outline) ------------------------------------
sd_tip_y   = -4.7;               // tongue tip when seated
sn_tongue  = [10.9, 24.0];
sn_body    = [28.0, 23.0];
sn_off_x   = 1.5;                // body centre offset from the tongue centre
sn_t       = 0.75;
z_sn_f     = z_pcbb - 0.25;      // 24.0 front face of the sniffer
z_sn_b     = z_sn_f - sn_t;      // 23.25
dup_len    = 14;                 // Dupont housing

// ---- AS3935 Grove board ---------------------------------------------------------------
sens       = [42.0, 23.0];       // incl. ears and the Grove socket lip (estimate)
sens_x0    = -15.75;             // Grove socket end (left)
sens_y0    = -0.5;
sens_lift  = 6.2;                // PCB underside above the lid (socket faces the lid)
sens_pcb   = 1.6;

// ---- Battery (902025 / 102525 class, JST 1.25 plug) ----------------------------------
bay_x      = [-16.6, 9.4];
bay_y      = [-28.0, -2.0];

// ---- Screws (M3 x 10, self-tapping into PETG) ----------------------------------------
boss_pos   = [[-21, -47], [21, -47]];
boss_d     = 6.5;
pilot_d    = 2.5;
screw_d    = 3.4;
head_d     = 6.2;
head_h     = 0.8;

// lid tongue at the top edge
tongue_w   = 16;
tongue_out = 1.0;
tongue_z   = [0.6, 1.6];

// ---- Decoration (engraved only; no functional geometry changes) ---------------------
eng_d      = 0.6;    // engraving depth on front face and lid outer face (3 layers: colour change at 0.6)
// front bezel (radar/watch style)
bez_ring_r = 24.0;   // outer ring centre radius
bez_ring_w = 0.8;    // outer ring line width
bez_n      = 24;     // tick count (every 15 deg)
bez_r0_maj = 19.6;   // major tick inner radius (window chamfer edge is at 17.75)
bez_r0_min = 20.8;   // minor tick inner radius
bez_r1     = 22.6;   // tick outer radius
bez_w_maj  = 1.2;    // major tick width (0/90/180/270)
bez_w_min  = 0.7;    // minor tick width
min_edge   = 2.5;    // min solid between any bezel engraving and the outer top edge incl. chamfer
// chin badge
badge_c    = [0, -36.5];  // hexagon centre
badge_r    = 7.0;         // hexagon circumradius (14 across corners, pointy-top)
badge_line = 0.9;         // hexagon outline width
badge_bolt = 8.5;         // bolt height inside the badge
chin_font  = "Avenir Next Condensed:style=Heavy";
chin_txt_y = -49.5;       // chin text centre
chin_txt_w = 31;          // chin text width (resized to fit)
chin_txt_h = 5.6;         // chin text height
cell_r     = [5.0, 3.2, 1.5];  // flanking honeycomb cells, circumradius, fading out
cell_gap   = 1.2;         // gap between flanking cells
cell_line  = [0.9, 0.7, 0];    // their outline widths (0 = solid dot)
// bee stripes on the side walls
stripe_z   = [3.5, 6.0, 8.5];  // centres, clear of USB-C (z 10.4+) and lever slot (z 18+)
stripe_h   = 1.0;         // groove height
stripe_d   = 0.45;        // groove depth (1 line); wall 2.25 keeps 1.8
// back lid honeycomb
hc_flat    = 5.0;         // cell across flats (raised island)
hc_web     = 1.2;         // engraved groove width between cells
hc_margin  = 1.5;         // solid margin to the lid edge
hc_keep_d  = 10;          // keep-out dia around each screw counterbore (head_d 6.2)
hex_big_r  = 18;          // plain central hexagon circumradius (pointy-top)
hex_big_w  = 1.0;         // its outline width
hex_big_c  = [0, -13.75]; // its centre (lid outline centre in Y)
lid_txt_w  = 24;          // lid text width
lid_txt_h  = 5.0;         // lid text height

echo(z_pcbb=z_pcbb, pivot=pivot, xiao_usb_z=xiao_usb_z,
     size=[2*half_w, top_y - bot_y, case_d]);
assert(z_sn_b - 2.5 - dup_len - 3 >= lid_t, "Dupont housings + wire bend do not fit");
assert(pocket_d > pcb_dia, "display pocket too small");
assert(top_y - (bez_ring_r + bez_ring_w / 2) - chamfer >= min_edge, "bezel ring too close to the top edge");
assert(bez_r0_maj - (window_d + 1.5) / 2 >= 1, "bezel ticks too close to the window chamfer");
assert(bez_r1 < bez_ring_r - bez_ring_w / 2, "ticks touch the bezel ring");
assert(stripe_z[0] - stripe_h / 2 >= 3 && stripe_z[2] + stripe_h / 2 <= 10, "stripes outside z 3..10");

// ======================================================================================
// 2D helpers
module outline(off = 0)
  offset(r = off) hull() {
    for (s = [-1, 1]) {
      translate([s * (half_w - r_top), top_y - r_top]) circle(r = r_top);
      translate([s * (half_w - r_bot), bot_y + r_bot]) circle(r = r_bot);
    }
  }

module ext(z0, z1) translate([0, 0, z0]) linear_extrude(z1 - z0) children();

function ang(v) = atan2(v[1], v[0]);

// ======================================================================================
// FRONT shell (print face down)
module front() {
  difference() {
    union() {
      // shell with a stepped front chamfer
      difference() {
        union() {
          ext(0, case_d - chamfer) outline();
          for (i = [0 : 4])
            ext(case_d - chamfer + i * 0.2 - eps, case_d - chamfer + (i + 1) * 0.2)
              outline(-(i + 1) * 0.2);
        }
        ext(-eps, z_fp_in) outline(-wall);
      }
      // display locating ring (gap at the FPC fold)
      difference() {
        ext(z_pcbb + 0.35, z_fp_in + eps) circle(d = pocket_d + 2 * ring_t);
        ext(z_pcbb, case_d) circle(d = pocket_d);
        ext(z_pcbb, case_d)
          polygon([[0, 0], [40 * cos(-ring_gap), 40 * sin(-ring_gap)],
                   [40, 0], [40 * cos(ring_gap), 40 * sin(ring_gap)]]);
      }
      // screw bosses
      for (p = boss_pos) translate(p) ext(lid_t, z_fp_in + eps) circle(d = boss_d);
      // lever keeper post + pivot pin
      translate(pivot) {
        ext(lev_z0 + lev_t + 0.2, z_fp_in + eps) circle(d = 5);
        ext(lev_z0 - 1.0, lev_z0 + lev_t + 0.3) circle(d = pivot_pin);
      }
      // sniffer hold-down ribs (stop 0.1 above its front face)
      for (x = [-4, 4])
        translate([x - 0.6, sd_tip_y - sn_tongue[1] - sn_body[1] + 1.5, z_sn_f + 0.1])
          cube([1.2, sn_body[1] - 3, z_fp_in - z_sn_f - 0.1 + eps]);
    }
    // display pocket + window + window chamfer
    ext(z_pcbb - 1, z_glass) circle(d = pocket_d);
    ext(z_glass - eps, case_d + 1) circle(d = window_d);
    translate([0, 0, case_d - 0.75]) cylinder(d1 = window_d, d2 = window_d + 1.5 + 2 * eps, h = 0.75 + eps);
    // engraved bezel + chin badge (front face, read from +Z, 0.6 deep)
    ext(case_d - eng_d, case_d + 1) { bezel2d(); chin2d(); }
    // bee stripes around the side walls
    for (z = stripe_z)
      ext(z - stripe_h / 2, z + stripe_h / 2)
        difference() { outline(1); outline(-stripe_d); }
    // USB-C opening in the top wall
    translate([0, top_y - wall - 1, xiao_usb_z])
      rotate([-90, 0, 0]) linear_extrude(wall + 2)
        offset(r = 2) square([usb_hole[0] - 4, usb_hole[1] - 4], center = true);
    // lever slot in the right wall
    translate([half_w - wall - 1, pivot[1] - slot_h / 2, lev_z0 - lev_drop - 0.2])
      cube([wall + 2, slot_h, lev_drop + 0.2 + tab[2] + 0.4]);
    // boss pilot holes
    for (p = boss_pos) translate(p) ext(lid_t - eps, z_fp_in - 3) circle(d = pilot_d);
    // lid tongue groove
    translate([0, 0, tongue_z[0] - 0.2])
      linear_extrude(tongue_z[1] - tongue_z[0] + 0.4)
        intersection() {
          outline(-wall + tongue_out + 0.2);
          translate([-(tongue_w + 2) / 2, 10]) square([tongue_w + 2, 30]);
        }
  }
}

module bolt2d(h)   // bold lightning bolt, centred, height h
  scale(h / 10) polygon([[-0.5, 5], [3, 5], [0.8, 1], [3.5, 1], [-2.5, -5], [-0.6, -0.8], [-3.2, -0.8]]);

module hex2d(r) rotate(30) circle(r = r, $fn = 6);     // pointy-top hexagon
module hexring2d(r, w) difference() { hex2d(r); offset(delta = -w) hex2d(r); }

// front bezel: outer ring + 24 ticks
module bezel2d() {
  difference() { circle(r = bez_ring_r + bez_ring_w / 2); circle(r = bez_ring_r - bez_ring_w / 2); }
  for (i = [0 : bez_n - 1]) rotate(i * 360 / bez_n) {
    major = (i % (bez_n / 4) == 0);
    r0 = major ? bez_r0_maj : bez_r0_min;
    w  = major ? bez_w_maj : bez_w_min;
    translate([r0, -w / 2]) square([bez_r1 - r0, w]);
  }
}

// chin: hexagon badge with bolt, flanking honeycomb cells, text
module chin2d() {
  translate(badge_c) {
    hexring2d(badge_r, badge_line);
    bolt2d(badge_bolt);
  }
  // flanking cells, edge to edge along the row, fading out
  for (s = [-1, 1]) {
    flank = badge_r * cos(30);                 // half flats of the badge
    for (i = [0 : len(cell_r) - 1]) {
      off = flank + cell_gap + sum_flats(i) + cell_r[i] * cos(30);
      translate([s * off, badge_c[1]])
        if (cell_line[i] > 0) hexring2d(cell_r[i], cell_line[i]); else hex2d(cell_r[i]);
    }
  }
  translate([0, chin_txt_y])
    resize([chin_txt_w, chin_txt_h])
      text("FLASH BEE", size = 8, font = chin_font, halign = "center", valign = "center");
}
// flats + gaps of the flanking cells before cell i
function sum_flats(i) = i == 0 ? 0 : sum_flats(i - 1) + 2 * cell_r[i - 1] * cos(30) + cell_gap;

// ======================================================================================
// BACK lid (print outer face down)
module back() {
  difference() {
    union() {
      ext(0, lid_t) outline(-wall - lid_clr);
      // top tongue
      ext(tongue_z[0], tongue_z[1])
        intersection() {
          outline(-wall + tongue_out);
          translate([-tongue_w / 2, 10]) square([tongue_w, 30]);
        }
      // display support posts with locating pins
      for (h = [hole_j8, hole_j6]) translate(h) {
        ext(lid_t - eps, z_pcbb - 0.1) circle(d = 4);
        ext(z_pcbb - 0.2, z_pcbb + pcb_t - 0.2) circle(d = pin_d);
      }
      // lever support post
      translate(pivot) ext(lid_t - eps, lev_z0 - 0.2) circle(d = 5);
      // sensor cradle
      sensor_cradle();
      // battery bay corners
      battery_bay();
    }
    // pivot pin socket
    translate(pivot) ext(lev_z0 - 1.25, lev_z0) circle(d = pivot_pin + 2 * 0.2);
    // screw holes with counterbore
    for (p = boss_pos) translate(p) {
      ext(-1, lid_t + 1) circle(d = screw_d);
      ext(-1, head_h) circle(d = head_d);
    }
    // engraved honeycomb + badge on the outer face; viewed from -Z, so mirrored in X
    ext(-1, eng_d) mirror([1, 0]) lid_art2d();
  }
}

// honeycomb grooves (cells stay raised), keep-outs, central plain hexagon with text + bolt
module lid_art2d() {
  p = hc_flat + hc_web;                          // cell pitch across flats
  rows = ceil((top_y - bot_y) / (p * sqrt(3) / 2)) + 2;
  cols = ceil(2 * half_w / p) + 2;
  intersection() {
    outline(-wall - lid_clr - hc_margin);
    union() {
      offset(delta = -0.3) offset(delta = 0.3)    // drop slivers under 0.6
      difference() {
        translate([-half_w - p, bot_y - p]) square([2 * half_w + 2 * p, top_y - bot_y + 2 * p]);
        for (j = [0 : rows], i = [0 : cols])
          translate([-half_w - p + i * p + (j % 2) * p / 2, bot_y - p + j * p * sqrt(3) / 2])
            hex2d(hc_flat / sqrt(3));
        for (q = boss_pos) translate(q) circle(d = hc_keep_d);
        translate(hex_big_c) offset(delta = hc_web) hex2d(hex_big_r);
      }
      translate(hex_big_c) {
        hexring2d(hex_big_r, hex_big_w);
        translate([0, 6.5]) bolt2d(12);
        translate([0, -7])
          resize([lid_txt_w, lid_txt_h])
            text("FLASH BEE", size = 8, font = chin_font, halign = "center", valign = "center");
      }
    }
  }
}

module sensor_cradle() {
  x0 = sens_x0; x1 = sens_x0 + sens[0];
  y0 = sens_y0; y1 = sens_y0 + sens[1];
  z_sup = lid_t + sens_lift;              // PCB underside
  // support ledges + outer walls along both long edges, a gap left for the Grove plug side
  for (side = [0, 1]) {
    yw = side == 0 ? y0 - 0.2 - 1.2 : y1 + 0.2;     // outer wall
    yl = side == 0 ? y0 - 0.2 : y1 - 1.0 + 0.2;     // ledge under the PCB edge
    for (xs = [[x0 + 8, x0 + 18], [x1 - 14, x1 - 2]]) {
      translate([xs[0], yl, lid_t - eps]) cube([xs[1] - xs[0], 1.0, z_sup - lid_t + eps]);
      translate([xs[0], yw, lid_t - eps]) cube([xs[1] - xs[0], 1.2, z_sup + sens_pcb + 0.8 - lid_t + eps]);
      // snap barb
      translate([xs[0] + 2, side == 0 ? yw + 1.2 - eps : yw + eps, z_sup + sens_pcb + 0.1])
        mirror([0, side == 0 ? 0 : 1, 0])
          rotate([90, 0, 90]) linear_extrude(xs[1] - xs[0] - 4)
            polygon([[0, 0], [0.45, 0], [0, 0.7]]);
    }
  }
  // the right wall stops the antenna end (0.5 mm)
}

module battery_bay() {
  t = 1.35; h = 5;
  for (x = [bay_x[0] - t, bay_x[1]])
    translate([x, bay_y[0] - t, lid_t - eps]) cube([t, 12, h]);
  translate([bay_x[0] - t, bay_y[0] - t, lid_t - eps]) cube([8, t, h]);
  translate([bay_x[1] - 8 + t, bay_y[0] - t, lid_t - eps]) cube([8, t, h]);
}

// ======================================================================================
// LEVER (print flat, as modelled: bottom at lev_z0)
module lever() {
  k = knob; p = pivot; u = knob_u; n = knob_n;
  a_u = ang(u);
  wall_x = half_w - wall;
  difference() {
    union() {
      // hub
      translate(p) ext(lev_z0, lev_z0 + lev_t) circle(d = hub_d);
      // arm pivot -> fork base (base sits outside the actuator tip)
      hull() {
        translate(p) ext(lev_z0, lev_z0 + lev_t) circle(d = 3);
        translate(k + 1.6 * n) ext(lev_z0, lev_z0 + lev_t) circle(d = 2.4);
      }
      // fork base bar along the slide direction
      translate(k + 1.6 * n) rotate([0, 0, a_u])
        translate([-(knob_w / 2 + 0.25 + 1.2), -0.6, lev_z0])
          cube([knob_w + 0.5 + 2.4, 1.2, lev_t]);
      // prongs, either side of the actuator
      for (s = [-1, 1])
        translate(k) rotate([0, 0, a_u])
          translate([s * (knob_w / 2 + 0.25 + 0.6) - 0.6, -1.6 - 0.6, lev_z0])
            cube([1.2, 2.2 + 0.6, prong_top - lev_z0]);
      // thumb arm to the right wall and the outside tab
      translate([p[0], p[1] - 1.2, lev_z0]) cube([wall_x + wall + 0.3 - p[0], 2.4, lev_t]);
      translate([wall_x + wall + 0.3, p[1] - tab[1] / 2, lev_z0])
        difference() {
          cube([tab[0], tab[1], tab[2]]);
          for (i = [0 : 2]) translate([tab[0] - 0.3, 0.6 + i * 1.3, -1]) cube([1, 0.5, tab[2] + 2]);
        }
    }
    translate(p) ext(lev_z0 - 1, lev_z0 + lev_t + 1) circle(d = pivot_pin + 2 * clr_free);
  }
}

// ======================================================================================
// FIT TEST: front plate ring around the display (lip, window, locating ring), ~15 min
module fit_test() {
  intersection() {
    front();
    ext(z_pcbb, case_d + 1) circle(d = pocket_d + 2 * ring_t + 4);
  }
}

// ======================================================================================
// Dummy parts for the assembly view and clearance checks
module dummy_display() {
  color("black") ext(z_pcbf + disp_gap, z_glass) circle(d = lcd_dia);
  color("dimgray") ext(z_pcbb, z_pcbf) difference() {
    circle(d = pcb_dia);
    translate([17.5, -8]) square([10, 16]);           // FPC notch
    for (h = [hole_j8, hole_j6, [15.621, 11.811]]) translate(h) circle(d = 2.2);
  }
  color("orange") translate(knob) ext(z_pcbb - 1.4, z_pcbb - 0.2)
    rotate(ang(knob_u)) square([knob_w, 1.3], center = true);
}
module dummy_xiao() {
  color("gold") for (x = [-7.62, 7.62]) translate([x - 1.25, 0.5, z_pcbb - 5]) cube([2.5, 18.3, 5]);
  color("black") for (x = [-7.62, 7.62]) translate([x - 1.25, 0.5, z_pcbb - 7.5]) cube([2.5, 18.3, 2.5]);
  color("darkgreen") translate([-8.9, -0.6, z_pcbb - 8.5]) cube([17.8, 21, 1.0]);
  color("silver") translate([-4.5, 13.9, z_pcbb - 8.5 - 3.2]) cube([9, 7.5, 3.2]);
}
module dummy_sniffer() {
  color("red") {
    translate([-sn_tongue[0] / 2, sd_tip_y - sn_tongue[1], z_sn_b]) cube([sn_tongue[0], sn_tongue[1], sn_t]);
    translate([sn_off_x - sn_body[0] / 2, sd_tip_y - sn_tongue[1] - sn_body[1], z_sn_b])
      cube([sn_body[0], sn_body[1], sn_t]);
  }
  hx = sn_off_x - sn_body[0] / 2 + 3.2;     // header row
  y0 = sd_tip_y - sn_tongue[1] - sn_body[1] + 1.5;
  color("black") translate([hx - 1.27, y0, z_sn_b - 2.5]) cube([2.54, 20.3, 2.5]);
  color("dimgray") translate([hx - 1.3, y0, z_sn_b - 2.5 - dup_len]) cube([2.6, 20.3, dup_len]);
}
module dummy_sensor() {
  z = lid_t + sens_lift;
  color("royalblue") translate([sens_x0, sens_y0, z]) cube([sens[0], sens[1], sens_pcb]);
  color("white") translate([sens_x0, sens_y0 + 6.5, z - 6]) cube([5.5, 10, 6]);
  color("white") translate([sens_x0 - 9, sens_y0 + 7, z - 5.4]) cube([9, 9, 5]);
}
module dummy_battery() {
  color("silver") translate([bay_x[0] + 0.5, bay_y[0] + 3, lid_t]) cube([25, 20, 9]);
}
module dummies() {
  dummy_display(); dummy_xiao(); dummy_sniffer(); dummy_sensor(); dummy_battery();
}

// ======================================================================================
if (part == "front")         translate([0, 0, case_d]) mirror([0, 0, 1]) front();   // face down
else if (part == "back")     back();
else if (part == "lever")    translate([0, 0, -lev_z0]) lever();
else if (part == "fit_test") translate([0, 0, case_d]) mirror([0, 0, 1]) fit_test();
else if (part == "front_asm") front();
else if (part == "exploded") {
  color("gold", 0.9) translate([0, 0, 30]) front();
  color("dimgray") translate([0, 0, -30]) back();
  color("orange") lever();
  dummies();
} else {
  color("gold", 0.35) front();
  color("dimgray") back();
  color("orange") lever();
  dummies();
}
