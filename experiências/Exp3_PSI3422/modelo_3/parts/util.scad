include <params.scad>

// Ferramenta de corte genérica: cilindro alto o bastante pra atravessar
// qualquer geometria local da chapa real (a maior parte é só 3mm, mas
// alguns pinos de alinhamento chegam a ~24mm — 80mm de altura cobre com
// folga, centralizado em Z pra não importar onde a chapa está).
module thru_hole(d) {
    translate([0, 0, -40]) cylinder(d = d, h = 80, $fn = 24);
}
