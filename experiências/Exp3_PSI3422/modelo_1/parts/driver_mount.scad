include <params.scad>

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

module driver_preview() {
    color("SteelBlue", 0.5)
        translate([driver_position[0], driver_position[1], driver_height / 2])
            cube([driver_footprint[0], driver_footprint[1], driver_height], center = true);
}
