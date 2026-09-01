include <parts/params.scad>
use <parts/legend.scad>
use <parts/base_plate.scad>
use <parts/kl25z_mount.scad>
use <parts/motor_mount.scad>
use <parts/driver_mount.scad>
use <parts/hw201_mount.scad>
use <parts/hcsr04_mount.scad>
use <parts/caster_mount.scad>
use <parts/battery_holder.scad>
use <parts/powerbank_mount.scad>
use <parts/pcb_mount.scad>

// "ASSEMBLY" -> preview 3D montado (relatório)
// "FLAT"     -> planificação 2D da chapa (corte a laser, DXF/SVG)
render_mode = "ASSEMBLY";

// No modelo_2 só a ponte H (driver) fica embaixo da chapa — igual ao
// rodízio, que já é estrutural (vem montado por baixo no kit). Todo o
// resto (placa, sensores, baterias, PCB) é montado no andar de cima.
// Motor/roda ficam fora do bloco "acima da chapa": no chassi real eles
// são passantes (o corte em cruz atravessa a placa) e a roda precisa
// tocar o chão, então sua base fica em z=0 (mesma referência do rodízio),
// não empilhada em cima da chapa.
module chassi_assembly() {
    base_plate();
    motor_bracket_preview(motor_x_position,  motor_y_offset);
    motor_bracket_preview(motor_x_position, -motor_y_offset);
    wheel_preview(motor_x_position,  motor_y_offset + wheel_width / 2);
    wheel_preview(motor_x_position, -(motor_y_offset + wheel_width / 2));
    translate([0, 0, plate_thickness]) {
        kl25z_board_preview();
        hw201_bracket_3d(motor_x_position,  motor_y_offset);
        hw201_bracket_3d(motor_x_position, -motor_y_offset);
        hcsr04_preview();
        battery9v_cradle_3d();
        if (pcb_on_upper_level) {
            pcb_standoff_3d();
            pcb_preview();
            powerbank_cradle_3d();
        }
    }
    driver_preview();
    caster_preview();
}

if (render_mode == "FLAT") {
    base_plate_2d();
} else {
    chassi_assembly();
}
