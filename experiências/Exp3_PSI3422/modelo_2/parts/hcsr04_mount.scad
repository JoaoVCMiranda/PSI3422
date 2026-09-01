include <params.scad>
use <legend.scad>

module hcsr04_mount() {
    translate(hcsr04_position)
        for (sy = [-1, 1])
            translate([0, sy * hcsr04_hole_spacing / 2])
                circle(d = hcsr04_hole_d);
}

module hcsr04_preview() {
    color(legend_color("hcsr04"))
        translate([hcsr04_position[0], hcsr04_position[1], 10])
            cube([hcsr04_footprint[0], hcsr04_footprint[1], 20], center = true);
}
