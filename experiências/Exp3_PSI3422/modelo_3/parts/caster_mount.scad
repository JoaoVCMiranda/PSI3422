include <params.scad>
use <legend.scad>

// Posição REAL (objeto "Caster" do .blend) — pendurado embaixo da
// chapa, sem precisar de furo cortado (o encaixe já existe na peça
// real, se necessário ajustar depois de medir a peça física).
module caster_preview() {
    color(legend_color("caster"))
        translate(caster_center)
            cylinder(d = (caster_footprint[0] + caster_footprint[1]) / 2, h = caster_footprint[2], center = true, $fn = 32);
}
