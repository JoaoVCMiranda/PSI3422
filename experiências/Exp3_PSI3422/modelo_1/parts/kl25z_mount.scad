include <params.scad>

// Reaproveita o gabarito de furação já existente em assets/ em vez de
// redigitar coordenadas de furos: projection() extrai o contorno 2D do
// STL plano. Durante $preview (F5) usamos um retângulo simples como
// substituto, porque projection()+import() no STL (1.4MB) é pesado no
// OpenSCAD 2021.01 e travaria a edição ao vivo.
module kl25z_mount() {
    translate(kl25z_position) rotate(kl25z_rotation) {
        if ($preview) {
            square(kl25z_footprint, center = true);
        } else {
            projection(cut = false) import(kl25z_drilling_guide_stl);
        }
    }
}

module kl25z_board_preview() {
    translate(kl25z_position) rotate(kl25z_rotation)
        import(kl25z_mounting_plate_stl);
}
