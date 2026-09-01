include <BOSL2/std.scad>

$fs = 0.5;
$fa = 2;

// ============================================================
// PARÂMETROS — modelo_3
//
// Mudança de abordagem em relação a modelo_1/2: a chapa é importada
// como SÓLIDO 3D real (assets/2wd_robot/chassis_plate.stl), sem
// achatar via projection()+linear_extrude e sem nenhuma rotação —
// usamos o referencial NATIVO do arquivo original (mesmo referencial
// do .blend baixado pelo usuário), para eliminar qualquer erro de
// transformação manual. Isso inverte length/width em relação a
// modelo_1/2:
//   X = largura do chassi (eixo lateral, ~149mm de ponta a ponta)
//   Y = comprimento do chassi / sentido de deslocamento (~210mm)
//   Z = altura (para cima)
//
// A posição dos motores e do rodízio vem de consulta direta ao
// .blend (bpy matrix_world.translation dos objetos "Motor",
// "Motor.001" e "Caster") — não são mais inferidas a partir de furos
// da chapa (essa inferência anterior, usada em modelo_1/2, estava
// desalinhada com a peça real, ver conversa).
// ============================================================

chassis_ref_stl       = "../assets/2wd_robot/chassis_plate.stl";

// --- Chapa: geometria real ---
// A chapa em si é fina (3mm, medido no histograma de vértices do STL:
// concentração maciça em Z=45 e Z=48, o resto são uns poucos pinos de
// alinhamento saindo dela) apesar da bounding box total (21.12-54.10)
// sugerir 33mm — isso é só a extensão desses pinos, não a chapa.
plate_top_z            = 48;    // REAL — superfície de cima da chapa
plate_bottom_z          = 45;    // REAL — superfície de baixo da chapa
plate_bbox_x            = [-76.36, 72.62];  // REAL, largura (X)
plate_bbox_y             = [-120.28, 89.69]; // REAL, comprimento (Y)

// --- Motores + rodas (posição REAL, lida do .blend) ---
// Os objetos "Motor"/"Motor.001" do .blend já incluem motor+roda como
// um único mesh (o Z bate exatamente com o diâmetro da roda). Usamos
// só a bounding box real deles para posicionar um cilindro (roda) +
// caixa (corpo do motor) simplificados — miniatura fiel o bastante,
// sem precisar importar os 16MB de cada malha real.
motor_l_center          = [-51.03, -32.97, 34.93]; // REAL (Motor)
motor_r_center           = [47.52, -32.97, 34.93]; // REAL (Motor.001)
motor_wheel_diameter      = 68.92; // REAL — bate com o Z-span do mesh (roda toca o chão em Z≈0.47)
motor_wheel_width          = 26;    // ASSUNÇÃO (largura da roda, não dá pra isolar do mesh combinado)
motor_body_footprint        = [37, 70, 23]; // ASSUNÇÃO (corpo do motor TT, X x Y x Z)

// --- Rodízio (posição REAL, lida do .blend) ---
caster_center             = [-2.00, 79.21, 25.77]; // REAL (objeto "Caster")
caster_footprint            = [38.00, 46.49, 39.98]; // REAL (dimensions do objeto)

// --- Ponte H L298N (embaixo da chapa) ---
l298n_footprint               = [43, 43, 26]; // spec típica do módulo L298N (incl. dissipador)
l298n_hole_inset                = 3;
l298n_position                    = [0, 10]; // área central aberta da chapa

// --- Sensor ultrassônico HC-SR04 ---
// CORRIGIDO no modelo_3: a maior dimensão (45mm) fica PERPENDICULAR
// ao sentido de deslocamento (Y) — ou seja, ao longo de X — e não mais
// alinhada com Y como em modelo_1/2. Fica na frente (Y alto, perto do
// rodízio) e voltado pra frente (+Y).
hcsr04_footprint_xy               = [45, 20]; // [X, Y] — 45 ao longo de X (perpendicular ao deslocamento)
hcsr04_hole_spacing                = 16;       // ao longo de X, mesmo eixo da maior dimensão
hcsr04_hole_d                       = 2.5;
hcsr04_height                        = 20;
hcsr04_position                       = [0, 42]; // entre a borda da PCB (y~30) e o rodízio (y~56) — frente da chapa

// --- FRDM-KL25Z ---
// Montada ACIMA do shield de PCB (não mais direto na chapa) — ver
// pcb_kl25z_gap abaixo.
kl25z_footprint                        = [53.34, 81.28];
kl25z_rotation                          = 0;
kl25z_drilling_guide_stl                 = "../assets/freescale_kl25z_drilling_guide.stl";
kl25z_mounting_plate_stl                  = "../assets/freescale_kl25z_mounting_plate.stl";

// --- PCB própria (shield, 10x10cm) ---
// Andar 1 (acima da chapa): PCB shield, sustentada por 4 pés impressos.
// Andar 2 (acima do shield): FRDM-KL25Z, empilhada por cima como um
// shield de Arduino.
pcb_footprint                            = [100, 100];
pcb_thickness                             = 1.6;
pcb_hole_d                                 = 3;
pcb_standoff_footprint                      = [90, 70]; // onde os 4 pés pousam na chapa
pcb_hole_inset                               = 8;
pcb_standoff_height                           = 30;  // chapa -> shield (folga da ponte H, que fica embaixo, e do L298N não interfere pq está embaixo tb)
pcb_position                                   = [0, -5]; // recuada p/ abrir espaço ao HC-SR04 na frente (ver hcsr04_position)
kl25z_standoff_height                           = 12;  // shield -> KL25Z (pequeno, tipo pino de shield)
kl25z_position_on_pcb                            = [0, 0]; // relativo ao centro da PCB

// --- HW-201 (par, um por roda) ---
// Cada um fica perto do respectivo motor/roda e aponta pra ela — a
// roda gira em torno do eixo X (perpendicular ao deslocamento Y), suas
// faces ficam voltadas para ±X, então o sensor "olha" ao longo de X.
hw201_footprint                                  = [10, 32]; // [ao longo de Y, altura]
hw201_hole_d                                      = 2.5;
hw201_y_offset_from_motor                          = 15; // desloca o sensor do motor em direção ao corpo (y = motor_y - offset)

// --- Módulo nRF24L01+ ---
// Encaixe simples (bolso de 3 paredes impresso) pra não ficar solto —
// só precisa posicionar a caixinha corretamente, sem parafusos.
nrf24_footprint                                     = [30.3, 14.5, 8]; // spec do módulo padrão c/ antena PCB
nrf24_pocket_wall                                     = 1.6;
nrf24_position_on_pcb                                  = [-30, 35]; // canto da PCB shield, relativo ao seu centro

// --- Baterias 9V (bocais/suportes, opcional — potência extra dos motores) ---
// Dimensão do SUPORTE completo (com tampa, sem plug — só a caixa),
// pesquisada em lojas de robótica BR: ~55x31x20mm por unidade.
battery9v_count                                        = 2;
battery9v_holder_footprint                              = [55, 31, 20]; // [X, Y, Z]
battery9v_spacing                                        = 35; // entre eixos, ao longo de X
battery9v_position                                        = [0, -75]; // área aberta atrás dos motores
