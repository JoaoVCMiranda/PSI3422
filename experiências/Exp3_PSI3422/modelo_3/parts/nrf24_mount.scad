include <params.scad>
use <legend.scad>
use <pcb_mount.scad>

// Bolso impresso de 3 paredes (topo aberto p/ os pinos/antena) sobre o
// deck da PCB shield — só precisa segurar o módulo no lugar, sem
// parafuso. "a caixinha posicionada corretamente basta."
function nrf24_world_position() = [
    pcb_position[0] + nrf24_position_on_pcb[0],
    pcb_position[1] + nrf24_position_on_pcb[1]
];

module nrf24_pocket_3d() {
    pos = nrf24_world_position();
    w = nrf24_pocket_wall;
    color(legend_color("nrf24"), 0.6)
        translate([pos[0], pos[1], pcb_top_z()])
            difference() {
                cube([nrf24_footprint[0] + 2 * w, nrf24_footprint[1] + 2 * w, nrf24_footprint[2]], center = true);
                translate([0, 0, w])
                    cube([nrf24_footprint[0], nrf24_footprint[1], nrf24_footprint[2]], center = true);
            }
}

module nrf24_preview() {
    pos = nrf24_world_position();
    color(legend_color("nrf24"))
        translate([pos[0], pos[1], pcb_top_z() + nrf24_footprint[2] / 2])
            cube(nrf24_footprint, center = true);
}
