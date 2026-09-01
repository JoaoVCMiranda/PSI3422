include <params.scad>
use <legend.scad>

module driver_mount_cutout() {
    translate(driver_position)
        for (sx = [-1, 1])
            for (sy = [-1, 1])
                translate([
                    sx * (driver_footprint[0] / 2 - driver_hole_inset),
                    sy * (driver_footprint[1] / 2 - driver_hole_inset)
                ])
                    circle(d = 3.2);
}

// No modelo_2 a ponte H é o único componente pendurado embaixo da chapa
// (todo o resto vai no andar de cima) — por isso o preview fica em -Z,
// espelhando o mesmo tratamento do rodízio.
module driver_preview() {
    color(legend_color("driver"))
        translate([driver_position[0], driver_position[1], -driver_height / 2])
            cube([driver_footprint[0], driver_footprint[1], driver_height], center = true);
}
