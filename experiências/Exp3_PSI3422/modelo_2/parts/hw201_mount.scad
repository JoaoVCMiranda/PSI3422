include <params.scad>
use <legend.scad>

module hw201_mount(motor_x, motor_y) {
    translate([motor_x + hw201_x_offset_from_motor, motor_y])
        for (sx = [-1, 1])
            translate([sx * hw201_footprint[0] / 2, 0])
                circle(d = hw201_hole_d);
}

// Suporte em L: parte do furo de fixação na chapa e se estende até a
// face interna da roda correspondente, com uma "cabeça" (olho do sensor
// IR) voltada para o disco da roda — um HW-201 por roda, cada um
// apontando para a sua.
module hw201_bracket_3d(motor_x, motor_y) {
    s = motor_y >= 0 ? 1 : -1;
    base = [motor_x + hw201_x_offset_from_motor, motor_y, 4];
    head = [motor_x, motor_y + s * (wheel_width / 2 + 3), wheel_diameter * 0.35];
    color(legend_color("hw201")) {
        hull() {
            translate(base) cube([hw201_footprint[0], hw201_footprint[1] / 3, 3], center = true);
            translate(head) cube([hw201_footprint[0] / 2, 4, 3], center = true);
        }
        translate(head) cube([hw201_footprint[0] * 0.6, 2, 6], center = true);
    }
}
