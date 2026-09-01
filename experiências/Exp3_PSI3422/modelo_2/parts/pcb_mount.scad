include <params.scad>
use <legend.scad>

// Andar superior: 4 pés impressos partindo da chapa base, sustentando a
// PCB 10x10cm acima de todos os componentes da chapa (bateria 9V deitada
// é o item mais alto, 17.5mm — folga garantida pelo pcb_standoff_height).
module pcb_mount() {
    translate(pcb_position)
        for (sx = [-1, 1])
            for (sy = [-1, 1])
                translate([
                    sx * (pcb_standoff_footprint[0] / 2 - pcb_hole_inset),
                    sy * (pcb_standoff_footprint[1] / 2 - pcb_hole_inset)
                ])
                    circle(d = pcb_hole_d);
}

module pcb_preview() {
    color(legend_color("pcb"))
        translate([pcb_position[0], pcb_position[1], pcb_standoff_height + pcb_thickness / 2])
            cube([pcb_footprint[0], pcb_footprint[1], pcb_thickness], center = true);
}

module pcb_standoff_3d() {
    color(legend_color("pcb_pe"))
        translate(pcb_position)
            for (sx = [-1, 1])
                for (sy = [-1, 1])
                    translate([
                        sx * (pcb_standoff_footprint[0] / 2 - pcb_hole_inset),
                        sy * (pcb_standoff_footprint[1] / 2 - pcb_hole_inset),
                        0
                    ])
                        cylinder(d = pcb_hole_inset * 1.6, h = pcb_standoff_height);
}
