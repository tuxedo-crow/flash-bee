// Flash Bee v3 exterior concept (looks only, no internals). Datum: display centre, Z0 = back face.
$fn = 96;
half_w = 29; top_y = 28.5; bot_y = -56;
split_z = 19;      // yellow face cover above, black body below
face_z  = 28;      // yellow face level
win_d   = 34;

module outline(o = 0)
  offset(r = 2.5 + o) offset(delta = -2.5)
    polygon([[-19, top_y], [19, top_y], [half_w, 18.5], [half_w, -10], [27, -15],
             [27, -45], [18, bot_y], [-18, bot_y], [-27, -45], [-27, -15],
             [-half_w, -10], [-half_w, 18.5]]);

module ext(z0, z1) translate([0, 0, z0]) linear_extrude(z1 - z0) children();

// keyhole frame: bezel ring + neck + chin panel frame
module keyhole_outer() {
  circle(r = 26.2);
  translate([-10, -40]) square([20, 20]);
  translate([0, -39]) offset(r = 4) square([22, 22], center = true);
}
module keyhole_inner_panel() translate([0, -39.5]) offset(r = 2.5) square([18, 17], center = true);

module body() color([0.13, 0.13, 0.14]) {
  // black body, back edges chamfered 4 mm so the device reads thinner
  hull() { ext(0, 0.01) outline(-4); ext(4, split_z) outline(); }
  // antenna pod behind the USB-C, centred: holds the AS3935 away from the C3 and LCD
  hull() {
    ext(1, 10) translate([0, top_y - 4]) offset(r = 3) square([22, 8], center = true);
    ext(2, 9)  translate([0, top_y + 14]) offset(r = 4) square([18, 4], center = true);
  }
}

module cover() {
  color([1.0, 0.78, 0.05]) difference() {
    union() {
      // yellow cover with a stepped front chamfer
      hull() { ext(split_z, face_z - 1.2) outline(); ext(face_z - 0.01, face_z) outline(-1.2); }
      // raised side grip bars
      for (s = [-1, 1]) for (i = [0 : 2])
        translate([s * (22.6 + i * 0), -21 - i * 8.5, face_z - 0.01])
          linear_extrude(1.0, scale = 0.9) offset(r = 0.6)
            polygon(s > 0 ? [[-1.2, 0], [1.8, 1.5], [1.8, -5], [-1.2, -6.5]]
                          : [[1.2, 0], [-1.8, 1.5], [-1.8, -5], [1.2, -6.5]]);
      // raised chin panel inside the frame
      ext(face_z - 0.01, face_z + 0.8) keyhole_inner_panel();
    }
    ext(face_z - 1.2, face_z + 2) offset(r = 0.3) difference() { keyhole_outer(); keyhole_inner_panel(); }
    ext(split_z, face_z + 2) circle(d = win_d);
  }
  // black inlay frame (second toolhead on the XL), proud 1.4 mm, bevelled inner edge on the bezel
  color([0.1, 0.1, 0.11]) difference() {
    ext(face_z - 1.2, face_z + 1.4) difference() { keyhole_outer(); keyhole_inner_panel(); }
    translate([0, 0, face_z - 2]) cylinder(d = win_d, h = 10);
    translate([0, 0, face_z + 0.2]) cylinder(d1 = win_d, d2 = win_d + 4.6, h = 1.21);
    // screw-head dots on the panel frame
    for (x = [-14.6, 14.6], y = [-27.5, -51.5]) translate([x, y, face_z + 1]) cylinder(d = 1.6, h = 1);
  }
  // glass (stand-in for the display)
  color([0.05, 0.08, 0.2]) ext(face_z - 1.5, face_z - 1.2) circle(d = win_d + 1);
}

part = "assembly";
body(); cover();
