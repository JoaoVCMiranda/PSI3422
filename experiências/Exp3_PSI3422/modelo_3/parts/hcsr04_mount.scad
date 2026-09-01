include <params.scad>
use <legend.scad>
use <util.scad>

// CORRIGIDO no modelo_3: maior dimensão (45mm) perpendicular ao sentido
// de deslocamento (Y) — ao longo de X — furos também espaçados em X.
module hcsr04_holes_3d() {
    translate([hcsr04_position[0], hcsr04_position[1], 0])
        for (sx = [-1, 1])
            translate([sx * hcsr04_hole_spacing / 2, 0])
                thru_hole(hcsr04_hole_d);
}

module hcsr04_preview() {
    color(legend_color("hcsr04"))
        translate([hcsr04_position[0], hcsr04_position[1], plate_top_z + hcsr04_height / 2])
            cube([hcsr04_footprint_xy[0], hcsr04_footprint_xy[1], hcsr04_height], center = true);
}
