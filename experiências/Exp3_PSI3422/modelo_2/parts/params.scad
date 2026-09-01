include <BOSL2/std.scad>

$fs = 0.5;
$fa = 2;

// ============================================================
// PARÂMETROS DO CHASSI — Chassi 2WD REF: 7MS10 (MakerHero)
// Fonte única de verdade (mm). Valores marcados // ASSUNÇÃO foram
// estimados a partir da foto (assets/desenho_chassi.png) e das specs
// publicadas do kit, não de medição direta na peça física — conferir
// com paquímetro antes de mandar cortar a chapa definitiva.
// ============================================================

// --- Placa base (acrílico, corte a laser) ---
// A silhueta real vem de assets/2wd_robot/chassis_plate.stl (extraída do
// projeto de referência baixado pelo usuário — objeto "RobotChassis.001"
// isolado do .blend original via Blender). plate_length/plate_width abaixo
// são só a bounding box dessa peça real, usados como placeholder rápido
// durante $preview — a silhueta de verdade (com TODOS os furos e recortes
// originais do kit) é importada diretamente em base_plate.scad.
chassis_ref_stl       = "../assets/2wd_robot/chassis_plate.stl";
plate_length          = 210;  // bounding box real (medida no STL de referência)
plate_width           = 149;  // bounding box real
plate_thickness       = 3;    // spec do kit — ASSUNÇÃO (STL de referência não informa espessura real)

// --- Tração ---
// Coordenadas REAIS lidas no STL de referência (não são mais estimativa):
// os dois recortes em cruz do motor ficam em x=-100.05, y=±(48.5~52.3).
// NOTA IMPORTANTE: esse afastamento (~100mm entre os centros dos motores)
// diverge do wheelbase de 200mm medido no carrinho dos Exp2/Exp4 — ou o
// carrinho real não é um 7MS10 padrão, ou o valor medido é outra referência.
// PENDENTE de reconciliação com o firmware de odometria.
wheelbase_measured_exp2_4 = 200; // só documentado, não usado no modelo
wheel_diameter        = 68;   // spec do kit — diverge do raio 5cm (Ø100mm) do firmware, ver nota acima
wheel_width            = 26;   // spec do kit
motor_x_position         = -100.05; // REAL (STL de referência)
motor_y_offset            = 50.4;    // REAL (média dos dois furos em cruz, ±48.53/±52.26)

// --- Motores DC (70x37x23) — os recortes em cruz já existem na peça real,
// não precisam ser cortados por este modelo; só usamos o footprint p/ o
// preview 3D do motor+suporte. ---
motor_footprint           = [70, 37, 23];

// --- KL25Z (reaproveita os STLs existentes) — posicionado na área aberta
// do corpo principal da chapa (nariz, sem flare, oposto aos motores). ---
kl25z_footprint             = [53.34, 81.28];
kl25z_position                = [35, 0];
kl25z_rotation                 = 0;
kl25z_drilling_guide_stl        = "../assets/freescale_kl25z_drilling_guide.stl";
kl25z_mounting_plate_stl        = "../assets/freescale_kl25z_mounting_plate.stl";

// --- HW-201 (par, um por roda, medindo disco de marcação) ---
hw201_footprint                = [10, 32];  // ASSUNÇÃO
hw201_hole_d                    = 2.5;
hw201_x_offset_from_motor         = 22;     // desloca o sensor do motor em direção ao corpo (x = motor_x + offset)

// --- HC-SR04 (padrão de furação confirmado no datasheet do repo) ---
hcsr04_hole_spacing              = 16;
hcsr04_footprint                   = [45, 20];
hcsr04_hole_d                       = 2.5;
hcsr04_position                       = [80, 0]; // ponta do nariz (extremidade sem flare), sensor voltado p/ frente

// --- Driver ponte H (já fixado no chassi real — ASSUNÇÃO tipo L298N) ---
driver_footprint                    = [43, 43];
driver_height                        = 15;   // ASSUNÇÃO
driver_hole_inset                     = 3;
driver_position                        = [-32, 0];

// --- Roda caster ---
// O furo real (redondo, Ø≈19.5mm, x=-91.3 y=0, bem à frente dos motores)
// já existe na peça — não precisamos cortar nada, só desenhar o preview 3D.
caster_position                            = [-91.3, 0]; // REAL (STL de referência)
caster_diameter                             = 19.5;       // REAL
caster_height                                = 25;         // ASSUNÇÃO, p/ preview

// --- Baterias 9V (2 ou 3, opcionais — potência extra dos motores) ---
// Deitadas (comprimento na horizontal) para minimizar altura sobre a chapa.
battery9v_count                             = 2;    // 2 ou 3, ajustável
battery9v_body_footprint                     = [48.5, 26.5, 17.5]; // NEDA 1604/IEC 6LR61, deitada
battery9v_spacing                             = 30;  // entre eixos, ao longo de Y
battery9v_position                             = [-55, 15];

// --- Powerbank 5V (lógica) ---
// Sem cutout na chapa: fica sobre o andar superior (deck da PCB), preso por
// velcro — mais tolerante à falta de modelo definido e evita competir por
// espaço na chapa já congestionada.
powerbank_footprint                             = [100, 35, 15]; // ASSUNÇÃO, ajustar ao modelo escolhido
powerbank_mount_style                            = "velcro_on_deck";
powerbank_deck_offset                             = [0, 40]; // posição sobre o deck, relativa ao centro da PCB

// --- PCB própria (10x10cm, pedido do usuário) ---
// Fica em um andar superior sobre espaçadores impressos, acima de todos os
// componentes da chapa base (a bateria 9V deitada é o item mais alto, 17.5mm).
pcb_footprint                                     = [100, 100];
pcb_thickness                                      = 1.6;   // espessura padrão de PCB
pcb_hole_d                                          = 3;
pcb_standoff_footprint                               = [90, 70]; // onde os 4 pés pousam na chapa
pcb_hole_inset                                        = 8;
pcb_standoff_height                                    = 30;  // > altura da bateria 9V deitada (17.5mm) + folga
pcb_position                                            = [-10, 0]; // centralizado sobre motores/driver/baterias
pcb_on_upper_level                                       = true;
