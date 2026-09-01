include <parts/params.scad>
use <parts/legend.scad>
use <parts/base_plate.scad>
use <parts/motor_mount.scad>
use <parts/caster_mount.scad>
use <parts/l298n_mount.scad>
use <parts/hw201_mount.scad>
use <parts/hcsr04_mount.scad>
use <parts/pcb_mount.scad>
use <parts/kl25z_mount.scad>
use <parts/nrf24_mount.scad>
use <parts/battery_holder.scad>

// Só "ASSEMBLY" nesse modelo — a chapa é o STL real importado
// diretamente (não uma silhueta 2D nossa), então não faz sentido um
// modo "FLAT" custom; a planificação pra corte é o próprio arquivo de
// referência assets/2wd_robot/chassis_plate.stl.
module chassi_assembly() {
    base_plate();

    // Estrutural: motores/rodas e rodízio na posição REAL (do .blend),
    // majoritariamente no subespaço abaixo da chapa (Z < plate_bottom_z).
    motor_body_preview(motor_l_center);
    motor_body_preview(motor_r_center);
    wheel_preview(motor_l_center);
    wheel_preview(motor_r_center);
    caster_preview();
    hw201_bracket_3d(motor_l_center);
    hw201_bracket_3d(motor_r_center);

    // Único componente embaixo da chapa: ponte H L298N.
    l298n_preview();

    // Acima da chapa: sensor, baterias, shield de PCB.
    hcsr04_preview();
    battery9v_preview();
    pcb_standoff_3d();
    pcb_preview();
    nrf24_pocket_3d();

    // Topo da pilha: FRDM-KL25Z empilhada acima do shield.
    kl25z_standoff_3d();
    kl25z_board_preview();
}

chassi_assembly();
