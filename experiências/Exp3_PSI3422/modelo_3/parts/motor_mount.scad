include <params.scad>
use <legend.scad>

// Motor + roda ficam nas posições REAIS lidas do .blend (objetos "Motor"
// e "Motor.001") — não são mais inferidos a partir de furos da chapa.
// A roda gira em torno do eixo X (perpendicular ao deslocamento em Y).
//
// O corpo do motor (caixa simplificada, ASSUNÇÃO — não vem do .blend)
// é deslocado pra baixo o suficiente pra não invadir o volume da chapa
// real (Z 45-48): sem essa folga a interseção exata caixa/chapa faz o
// CGAL travar no --render (união com face coincidente é um caso
// degenerado clássico).
module motor_body_preview(center) {
    body_top_z = plate_bottom_z - 1; // 1mm de folga da chapa
    body_z = body_top_z - motor_body_footprint[2] / 2;
    color(legend_color("motor"))
        translate([center[0], center[1], body_z])
            cube(motor_body_footprint, center = true);
}

module wheel_preview(center) {
    color(legend_color("roda"))
        translate(center)
            rotate([0, 90, 0])
                cylinder(d = motor_wheel_diameter, h = motor_wheel_width, center = true, $fn = 48);
}
