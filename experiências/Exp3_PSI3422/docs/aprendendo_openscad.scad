// Aprendendo OpenSCAD -- Exp3_PSI3422
//
// Abra este arquivo no editor do OpenSCAD (nao no navegador de STL) e
// aperte F5 (Preview) toda vez que mudar algo -- F5 e rapido mas nao
// garante que a geometria e "manifold" (fechada/valida); F6 (Render)
// faz o calculo CSG de verdade e e o que exportar STL exige, mas e
// bem mais lento -- use F5 enquanto edita, F6 so no final.
//
// Troque SECAO abaixo (1 a 7) pra pular entre os exemplos -- cada
// bloco `if (SECAO == N)` esta comentado explicando o conceito.

SECAO = 6;

/* -----------------------------------------------------------------
 * 1) Primitivas + $fn
 * cube([x,y,z], center=bool), cylinder(h=,r=,center=), sphere(r=).
 * $fn controla quantas facetas um circulo/esfera usa -- baixo (16-32)
 * pra preview rapido, alto (64+) so quando for exportar/imprimir.
 * --------------------------------------------------------------- */
if (SECAO == 1) {
    cube([20, 10, 5], center = true);
    translate([0, 20, 0]) cylinder(h = 5, r = 6, center = true, $fn = 32);
    translate([0, 40, 0]) sphere(r = 6, $fn = 32);
}

/* -----------------------------------------------------------------
 * 2) Transformacoes se compoem de dentro pra fora
 * `translate(a) rotate(b) objeto();` primeiro roda o objeto (em
 * torno da ORIGEM), so depois desloca o resultado inteiro -- a
 * transformacao mais perto do objeto e a que "acontece primeiro".
 * --------------------------------------------------------------- */
if (SECAO == 2) {
    // desloca, DEPOIS roda em torno do proprio centro ja deslocado
    color("red") translate([30, 0, 0]) rotate([0, 0, 45]) cube([20, 5, 5]);

    // roda em torno da origem, DEPOIS desloca -- vira uma "orbita"
    color("blue") rotate([0, 0, 45]) translate([30, 0, 0]) cube([20, 5, 5]);
}

/* -----------------------------------------------------------------
 * 3) Booleanas: union / difference / intersection
 * A chapa de um chassi e basicamente isso: contorno externo MENOS
 * os furos de fixacao (difference). O primeiro filho e a "base", os
 * seguintes sao subtraidos dele.
 * --------------------------------------------------------------- */
if (SECAO == 3) {
    difference() {
        cube([40, 40, 5], center = true);
        cylinder(h = 10, r = 10, center = true, $fn = 48);
        translate([15, 15, 0]) cylinder(h = 10, r = 3, center = true, $fn = 24);
        translate([-15, 15, 0]) cylinder(h = 10, r = 3, center = true, $fn = 24);
        translate([15, -15, 0]) cylinder(h = 10, r = 3, center = true, $fn = 24);
        translate([-15, -15, 0]) cylinder(h = 10, r = 3, center = true, $fn = 24);
    }
}

/* -----------------------------------------------------------------
 * 4) 2D -> 3D com linear_extrude -- o jeito certo de modelar uma
 * CHAPA (tipo o chassi): desenha o contorno em 2D (barato de editar,
 * facil de ver de cima) e extruda uma vez, em vez de mexer em 3D
 * direto. hull() aqui faz o contorno arredondado tipo "pista de
 * atletismo" -- parecido com o formato da foto em assets/desenho_chassi.png.
 * --------------------------------------------------------------- */
if (SECAO == 4) {
    linear_extrude(height = 3) {
        hull() {
            translate([-30, 0]) circle(r = 20, $fn = 64);
            translate([30, 0]) circle(r = 20, $fn = 64);
        }
    }
}

/* -----------------------------------------------------------------
 * 5) Modulos + for -- nao repita o mesmo cylinder() 4x com numeros
 * diferentes; defina um modulo de furo uma vez e chame num loop com
 * as posicoes. E o padrao certo pros furos de parafuso repetidos
 * (tipo os da chapa do KL25Z em assets/freescale_kl25z_drilling_guide.stl).
 * --------------------------------------------------------------- */
module furo_m3(x, y) {
    translate([x, y, -1]) cylinder(h = 10, r = 1.6, $fn = 20);
}

if (SECAO == 5) {
    posicoes = [[-15, 15], [15, 15], [-15, -15], [15, -15]];
    difference() {
        linear_extrude(height = 3) square([40, 40], center = true);
        for (p = posicoes) furo_m3(p[0], p[1]);
    }
}

/* -----------------------------------------------------------------
 * 6) import() de STL como GABARITO de alinhamento -- e o que
 * chassi.scad ja faz hoje com os dois STL da Freescale. O modificador
 * "%" na frente da peca faz ela aparecer cinza-transparente no
 * preview e ser IGNORADA em difference()/union()/render -- serve pra
 * alinhar visualmente sem a peca virar geometria de verdade. Tire o
 * "%" quando quiser que ela realmente corte/some na peca.
 * --------------------------------------------------------------- */
if (SECAO == 6) {
    % import("assets/freescale_kl25z_drilling_guide.stl");
    // a chapa de verdade seria desenhada aqui embaixo, alinhada
    // visualmente contra o gabarito transparente acima
    translate([0, 0, -5]) cube([80, 60, 3], center = true);
}

/* -----------------------------------------------------------------
 * 7) Exportar pra STL
 * Preview (F5) so desenha uma aproximacao rapida -- nao valida se a
 * peca fechou direito (nao roda o CSG/CGAL de verdade). Antes de
 * exportar:
 *   1. F6 (Render) -- calcula a malha final, mais lento que F5.
 *      Olhe o console: se aparecer algo tipo "WARNING: ... not a
 *      valid 2-manifold", a peca tem furo/superficie degenerada e
 *      pode falhar no fatiador -- geralmente causado por faces
 *      coincidentes numa difference()/union() (por isso furo_m3, na
 *      secao 5 acima, comeca em z=-1 e vai alem da chapa dos dois
 *      lados -- nunca encosta exatamente na borda que deveria
 *      atravessar; regra geral: prefira sobrepor demais a encostar
 *      exato).
 *   2. File > Export > Export as STL (so libera depois do F6).
 *
 * Sem abrir a GUI, o mesmo processo em uma linha (foi assim que as
 * imagens deste tutorial foram geradas, so trocando -o por --imgsize):
 *   openscad -o chapa.stl -D SECAO=7 aprendendo_openscad.scad
 *
 * OpenSCAD nao tem unidade fixa -- por convencao (e pra bater com
 * fatiador/impressora) trate todo numero aqui como milimetro.
 * --------------------------------------------------------------- */
if (SECAO == 7) {
    posicoes = [[-15, 15], [15, 15], [-15, -15], [15, -15]];
    difference() {
        linear_extrude(height = 3) square([40, 40], center = true);
        for (p = posicoes) furo_m3(p[0], p[1]);
    }
}
