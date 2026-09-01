include <params.scad>
use <legend.scad>
use <util.scad>

// A roda gira em torno do eixo X (perpendicular ao deslocamento em Y),
// então sua face fica voltada para dentro (+-X). Cada suporte parte de
// um furo de fixação na chapa e se estende até a face interna da roda
// correspondente — um HW-201 por roda, cada um apontando para a sua.
module hw201_holes_3d(motor_center) {
    inward = motor_center[0] >= 0 ? -1 : 1;
    base = [motor_center[0] + inward * 25, motor_center[1] - hw201_y_offset_from_motor];
    translate(base)
        for (sy = [-1, 1])
            translate([0, sy * hw201_footprint[0] / 2])
                thru_hole(hw201_hole_d);
}

module hw201_bracket_3d(motor_center) {
    inward = motor_center[0] >= 0 ? -1 : 1;
    base = [motor_center[0] + inward * 25, motor_center[1] - hw201_y_offset_from_motor, plate_top_z + 4];
    head = [motor_center[0] + inward * (motor_wheel_width / 2 + 3), motor_center[1], motor_center[2]];
    color(legend_color("hw201")) {
        hull() {
            translate(base) cube([hw201_footprint[0] / 3, hw201_footprint[0], 3], center = true);
            translate(head) cube([4, hw201_footprint[0] / 2, 3], center = true);
        }
        // "olho" do sensor, voltado para a face da roda (normal em X)
        translate(head) cube([2, hw201_footprint[0] * 0.6, 6], center = true);
    }
}
