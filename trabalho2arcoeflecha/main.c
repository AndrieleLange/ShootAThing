#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include "nokia5110.h"

#define BOTAO_CIMA    PD7
#define BOTAO_BAIXO   PB0
#define BOTAO_ATIRAR  PD0
#define MAX_TIROS     5
#define TAMANHO_BASE  3
#define TAMANHO_FLECHA 5
#define TAMANHO_PIXEL 3

volatile int baseX, baseY; // Posição da base (linha vertical)
volatile int flechaX, flechaY; // Posição da flecha (linha horizontal)
volatile double anguloFlecha = 0;
volatile int raioFlecha = 20;
volatile int v = 0;
volatile int acertos = 0;
volatile int tempo = 0;
volatile bool iniciado = false;

typedef struct {
    int x;
    int y;
    bool ativo;
} Tiro;

typedef struct {
    int x;
    int y;
    bool ativo;
} Pixel;

volatile Pixel pixel; // Informações do pixel

volatile Tiro tiros[MAX_TIROS];

void lcd_atualizar();
void movimentaBaseEFlecha();
void atirar();
void atualizarTiro(int indice);
void reiniciarTiros();
void gerarNovoPixel();
void timer_init();
void aguardarBotaoPressionado();

void lcd_atualizar() {
    nokia_lcd_clear();
    
    if (!iniciado) {
        nokia_lcd_set_cursor(1, 1);
        nokia_lcd_write_string("SHOOT A THING", 1);
        nokia_lcd_set_cursor(1, 12);
        nokia_lcd_write_string("PRESS SHOOT", 1);

        nokia_lcd_render();
        
        while (PIND & (1 << PD0)) {
            _delay_ms(50);
        }

        iniciado = true;
    } else {
        if (tempo >= 10000) {
            nokia_lcd_clear();
            nokia_lcd_write_string("GAME OVER", 1);
            nokia_lcd_render();
            _delay_ms(2000);
            tempo = 0;
            acertos = 0;
            iniciado = false;
            gerarNovoPixel(); // Gera novas posições para o pixel
            reiniciarTiros(); // Reinicia os tiros
            aguardarBotaoPressionado(); // Aguarda o botão ser pressionado novamente
        } else if (acertos >= 10) {
            nokia_lcd_clear();
            nokia_lcd_write_string("VOCÊ VENCEU!", 1);
            nokia_lcd_render();
            _delay_ms(2000);
            iniciado = false;
            tempo = 0;
            acertos = 0;

            // Aguarda o botão de atirar ser pressionado para reiniciar o jogo
            while (PIND & (1 << PD0)) {
                v++;
            }
            gerarNovoPixel(); // Gera novas posições para o pixel
            reiniciarTiros(); // Reinicia os tiros
        } else {
            // Restante do código permanece o mesmo...
            
            // Desenha a base (linha vertical)
            line_lcd(baseX, baseY - TAMANHO_BASE, baseX, baseY + TAMANHO_BASE);

            // Desenha a flecha (linha horizontal)
            line_lcd(flechaX - TAMANHO_FLECHA, flechaY, flechaX + TAMANHO_FLECHA, flechaY);

            // Desenha o pixel se estiver ativo
            if (pixel.ativo) {
                for (int i = 0; i < TAMANHO_PIXEL; i++) {
                    for (int j = 0; j < TAMANHO_PIXEL; j++) {
                        nokia_lcd_set_pixel(pixel.x + i, pixel.y + j, 1);
                    }
                }
            }

            // Atualiza os tiros se estiverem ativos
            for (int i = 0; i < MAX_TIROS; i++) {
                if (tiros[i].ativo) {
                    nokia_lcd_set_pixel(tiros[i].x, tiros[i].y, 1);
                    atualizarTiro(i);
                }
            }

            // Verifica se um dos tiros atingiu o pixel
            for (int i = 0; i < MAX_TIROS; i++) {
                if (tiros[i].ativo && tiros[i].x >= pixel.x && tiros[i].x <= pixel.x + TAMANHO_PIXEL &&
                    tiros[i].y >= pixel.y && tiros[i].y <= pixel.y + TAMANHO_PIXEL) {
                    tiros[i].ativo = false; // Desativa o tiro
                    acertos++;
                    gerarNovoPixel(); // Gera um novo pixel
                }
            }

            // Verifica se o pixel atingiu as bordas da tela ou está próximo dos contadores
            if (pixel.ativo && ((pixel.x <= 0 || pixel.x >= 83 || pixel.y <= 0 || pixel.y >= 47) ||
                (pixel.x >= baseX - TAMANHO_BASE && pixel.x <= baseX + TAMANHO_BASE && pixel.y >= baseY - TAMANHO_BASE && pixel.y <= baseY + TAMANHO_BASE) ||
                (pixel.x >= flechaX - TAMANHO_FLECHA && pixel.x <= flechaX + TAMANHO_FLECHA && pixel.y >= flechaY - TAMANHO_FLECHA && pixel.y <= flechaY + TAMANHO_FLECHA))) {
                gerarNovoPixel(); // Gera um novo pixel
            }

            // Mostra o contador de acertos na tela
            char acertos_str[15];
            sprintf(acertos_str, "\n\n\n\n\n\n\nPts:%d", acertos);
            nokia_lcd_set_cursor(1, 1);
            nokia_lcd_write_string(acertos_str, 1);

            // Mostra o contador de tempo na tela
            char tempo_str[15];
            sprintf(tempo_str, "Time:%d", tempo);
            nokia_lcd_set_cursor(1, 1);
            nokia_lcd_write_string(tempo_str, 1);
        }
    }

    nokia_lcd_render();
}

void movimentaBaseEFlecha() {
    if (PIND & (1 << PD7)) {
        baseY -= 2; // Move a base para cima
        flechaY -= 2; // Move a flecha para cima
    }

    if (PINB & (1 << PB0)) {
        baseY += 2; // Move a base para baixo
        flechaY += 2; // Move a flecha para baixo
    }
}

void atirar() {
    static bool botaoAtirarAnterior = true; // Estado anterior do botão de atirar
    bool botaoAtirarAtual = PIND & (1 << PD0); // Estado atual do botão de atirar

    if (botaoAtirarAnterior && !botaoAtirarAtual) {
        for (int i = 0; i < MAX_TIROS; i++) {
            if (!tiros[i].ativo) {
                // Inicializa o tiro na ponta da flecha
                tiros[i].x = flechaX + TAMANHO_FLECHA; // Ajuste conforme necessário
                tiros[i].y = flechaY;
                tiros[i].ativo = true;
                break; // Só ativa um tiro por vez
            }
        }
    }

    botaoAtirarAnterior = botaoAtirarAtual; // Atualiza o estado anterior
}
void aguardarBotaoPressionado() {
    while (PIND & (1 << PD0)) {
        // Aguarda até que o botão PD0 seja pressionado
        _delay_ms(50);
    }
}
void atualizarTiro(int indice) {
    // Move o tiro para a direita
    tiros[indice].x += 2;

    // Verifica se o tiro atingiu as bordas da tela
    if (tiros[indice].x >= 83 || tiros[indice].y <= 0 || tiros[indice].y >= 47) {
        nokia_lcd_clear();
        nokia_lcd_write_string("GAME OVER", 1);
        nokia_lcd_render();
        _delay_ms(2000);
        tempo = 0;
        acertos = 0;
        gerarNovoPixel(); // Gera novas posições para o pixel
        reiniciarTiros(); // Reinicia os tiros
        aguardarBotaoPressionado(); // Aguarda o botão ser pressionado novamente
    } else {
        // Verifica se o tiro atingiu o pixel
        if (tiros[indice].ativo && tiros[indice].x >= pixel.x && tiros[indice].x <= pixel.x + TAMANHO_PIXEL &&
            tiros[indice].y >= pixel.y && tiros[indice].y <= pixel.y + TAMANHO_PIXEL) {
            tiros[indice].ativo = false; // Desativa o tiro
            acertos++;
            gerarNovoPixel(); // Gera um novo pixel
        }
    }
}
void reiniciarTiros() {
    for (int i = 0; i < MAX_TIROS; i++) {
        tiros[i].ativo = false;
    }
}

void gerarNovoPixel() {
    // Gera novas coordenadas para o pixel
    do {
        pixel.x = 5 + (rand(v) % (83 - TAMANHO_PIXEL - 5));
        pixel.y = 5 + (rand(v) % (47 - TAMANHO_PIXEL - 5));
    } while (
        // Verifica se o pixel está muito próximo da flecha
        (pixel.x >= flechaX - TAMANHO_FLECHA && pixel.x <= flechaX + TAMANHO_FLECHA &&
         pixel.y >= flechaY - TAMANHO_FLECHA && pixel.y <= flechaY + TAMANHO_FLECHA) ||
        // Verifica se o pixel está acima da metade da tela
        (pixel.y <= 47 / 2) ||
        // Verifica se o pixel está à esquerda da metade da tela
        (pixel.x <= 83 / 2)
    );

    pixel.ativo = true;
}


ISR(TIMER1_COMPA_vect) {
    tempo += 100; // Incrementa o tempo a cada 100 ms
}

void timer_init() {
    TCCR1B |= (1 << WGM12); // Configura o modo CTC
    OCR1A = 15624; // Configura o valor de comparação para 1 segundo
    TIMSK1 |= (1 << OCIE1A); // Habilita a interrupção de comparação
    TCCR1B |= (1 << CS10) | (1 << CS12); // Configura o prescaler para 1024
}

int main() {
    // Configuração dos botões
    DDRD &= ~(_BV(BOTAO_ATIRAR));
    PORTD |= _BV(BOTAO_ATIRAR);
    DDRD &= ~(_BV(BOTAO_CIMA));
    PORTD |= _BV(BOTAO_CIMA);
    DDRB &= ~(_BV(BOTAO_BAIXO));
    PORTB |= _BV(BOTAO_BAIXO);
    sei();
    nokia_lcd_init();

    // Inicialização do timer
    timer_init();

    // Inicialização da base e da flecha
    baseX = 10;
    baseY = 24;
    flechaX = 10;
    flechaY = 24;

    // Inicialização dos tiros e do pixel
    reiniciarTiros();
    gerarNovoPixel();

    while (1) {
        movimentaBaseEFlecha();
        atirar();
        lcd_atualizar();
        _delay_ms(50);
    }
}
