include <params.scad>
use <legend.scad>

// Os recortes em cruz do motor já existem na peça real (importados junto
// com a silhueta em base_plate.scad) — aqui só o preview 3D do motor+roda,
// posicionado nas coordenadas reais lidas do STL de referência.
module motor_bracket_preview(x, y) {
    color(legend_color("motor"))
        translate([x, y, motor_footprint[2] / 2])
            cube(motor_footprint, center = true);
}

module wheel_preview(x, y) {
    color(legend_color("roda"))
        translate([x, y, wheel_diameter / 2])
            rotate([90, 0, 0])
                cylinder(d = wheel_diameter, h = wheel_width, center = true);
}
