include <params.scad>
use <legend.scad>
use <util.scad>

module l298n_holes_3d() {
    translate([l298n_position[0], l298n_position[1], 0])
        for (sx = [-1, 1])
            for (sy = [-1, 1])
                translate([
                    sx * (l298n_footprint[0] / 2 - l298n_hole_inset),
                    sy * (l298n_footprint[1] / 2 - l298n_hole_inset)
                ])
                    thru_hole(3.2);
}

// Único componente pendurado embaixo da chapa (Z negativo).
module l298n_preview() {
    color(legend_color("l298n"))
        translate([l298n_position[0], l298n_position[1], plate_bottom_z - l298n_footprint[2] / 2])
            cube(l298n_footprint, center = true);
}
