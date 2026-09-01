include <params.scad>
use <legend.scad>
use <pcb_mount.scad>

// Os STLs freescale_kl25z_*.stl NÃO são centrados na própria origem
// (bbox real: X -71.12..-17.78, Y 3.81..85.09) — sem essa correção de
// -44.45/+44.45 o import fica deslocado ~44mm do ponto pretendido.
kl25z_stl_offset = [44.45, -44.45];

// A KL25Z fica empilhada ACIMA do shield de PCB (não mais direto na
// chapa como em modelo_1/2) — pequenos pés impressos entre o shield e
// a placa, tipo pilha de shield de Arduino.
function kl25z_world_position() = [
    pcb_position[0] + kl25z_position_on_pcb[0],
    pcb_position[1] + kl25z_position_on_pcb[1]
];

module kl25z_standoff_3d() {
    color(legend_color("pcb_pe"))
        translate([kl25z_world_position()[0], kl25z_world_position()[1], pcb_top_z()])
            for (sx = [-1, 1])
                translate([sx * kl25z_footprint[0] * 0.35, 0])
                    cylinder(d = 5, h = kl25z_standoff_height, $fn = 16);
}

module kl25z_board_preview() {
    pos = kl25z_world_position();
    color(legend_color("kl25z"))
        translate([pos[0], pos[1], pcb_top_z() + kl25z_standoff_height])
            rotate(kl25z_rotation)
                translate(kl25z_stl_offset)
                    import(kl25z_mounting_plate_stl);
}
