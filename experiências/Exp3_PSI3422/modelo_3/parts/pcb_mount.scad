include <params.scad>
use <legend.scad>
use <util.scad>

// Andar 1: PCB shield sustentada por 4 pés impressos partindo da chapa
// real. Andar 2 (ver kl25z_mount.scad): FRDM-KL25Z empilhada acima do
// shield, como um shield de Arduino.
module pcb_standoff_holes_3d() {
    translate(pcb_position)
        for (sx = [-1, 1])
            for (sy = [-1, 1])
                translate([
                    sx * (pcb_standoff_footprint[0] / 2 - pcb_hole_inset),
                    sy * (pcb_standoff_footprint[1] / 2 - pcb_hole_inset)
                ])
                    thru_hole(pcb_hole_d);
}

module pcb_standoff_3d() {
    color(legend_color("pcb_pe"))
        translate([pcb_position[0], pcb_position[1], plate_top_z])
            for (sx = [-1, 1])
                for (sy = [-1, 1])
                    translate([
                        sx * (pcb_standoff_footprint[0] / 2 - pcb_hole_inset),
                        sy * (pcb_standoff_footprint[1] / 2 - pcb_hole_inset),
                        0
                    ])
                        cylinder(d = pcb_hole_inset * 1.6, h = pcb_standoff_height);
}

module pcb_preview() {
    color(legend_color("pcb"))
        translate([pcb_position[0], pcb_position[1], plate_top_z + pcb_standoff_height + pcb_thickness / 2])
            cube([pcb_footprint[0], pcb_footprint[1], pcb_thickness], center = true);
}

// Topo do shield (onde a KL25Z e o nRF24 se apoiam) — usado por
// kl25z_mount.scad e nrf24_mount.scad para empilhar acima da PCB.
function pcb_top_z() = plate_top_z + pcb_standoff_height + pcb_thickness;
