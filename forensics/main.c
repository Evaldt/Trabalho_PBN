#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Para usar strings
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

// Um pixel Pixel (24 bits)
typedef struct
{
    unsigned char r, g, b;
} Pixel;

// Uma imagem Pixel
typedef struct
{
    int width, height;
    int channels;
    Pixel *pixels;
} Img;

// As 2 imagens
Img in, out;

// Protótipos
void load(char *name, Img *pic);
void draw_line(int width, int height, Pixel img[][width], int x0, int y0, int x1, int y1, Pixel color, int thickness);

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("forensics [origem]\n");
        exit(1);
    }

    // Carrega a imagem original
    load(argv[1], &in);

    // Exibe as dimensões na tela, para conferência
    printf("Origem   : %s %d x %d\n", argv[1], in.width, in.height);

    printf("Processando...\n");

    // Cria imagem de saída e "zera" ela
    int tam = in.width * in.height;
    out = in;
    out.pixels = malloc(tam * sizeof(Pixel));
    memset(out.pixels, 0, tam * sizeof(Pixel));

    // Converte para interpretar como matrizes
    Pixel (*pin)[in.width] = (Pixel(*)[in.height]) in.pixels;
    Pixel (*pout)[in.width] = (Pixel(*)[in.height]) out.pixels;

    //
    // Neste ponto, voce deve implementar o algoritmo!
    // (ou chamar funcoes para fazer isso)
    //
    // Aplica o algoritmo em pin e gera a saida em pout
    // ...
    
    const int BLOCK = 16;
    const int STEP = 8;
    const int MIN_DISTANCE = 48;
    const int MAX_MATCHES = 18;
    const int MIN_STDDEV = 18;
    const int MIN_EDGE_MEAN = 22;
    const int MIN_EDGE_RATIO = 10;
    const int MATCH_THRESHOLD = 220;

    typedef struct {
        int media;
        int desvio_padrao;
        int media_bordas;
        int razao_bordas;
        unsigned char textura[16];
        unsigned char borda_quadrante[4];
    } BlocoInfo;

    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            unsigned char lum =
                (unsigned char)(0.59 * pin[y][x].g + 0.30 * pin[y][x].r + 0.11 * pin[y][x].b);

            pout[y][x].r = lum;
            pout[y][x].g = lum;
            pout[y][x].b = lum;
        }
    }

    unsigned char cinza[in.height][in.width];
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            Pixel p = pin[y][x];
            cinza[y][x] = (unsigned char)(0.59 * p.g + 0.30 * p.r + 0.11 * p.b);
        }
    }

    unsigned char bordas[in.height][in.width];
    for (int y = 1; y < in.height - 1; y++) {
        for (int x = 1; x < in.width - 1; x++) {
            int gx =
                -cinza[y - 1][x - 1] + cinza[y - 1][x + 1]
                -2 * cinza[y][x - 1] + 2 * cinza[y][x + 1]
                -cinza[y + 1][x - 1] + cinza[y + 1][x + 1];

            int gy =
                -cinza[y - 1][x - 1] - 2 * cinza[y - 1][x] - cinza[y - 1][x + 1]
                +cinza[y + 1][x - 1] + 2 * cinza[y + 1][x] + cinza[y + 1][x + 1];

            int mag = abs(gx) + abs(gy);

            if (mag > 255)
                mag = 255;

            bordas[y][x] = mag;
        }
    }

    Pixel red = {255, 0, 0};

    int grade_x = ((in.width - BLOCK) / STEP) + 1;
    int grade_y = ((in.height - BLOCK) / STEP) + 1;
    int total_blocos = grade_x * grade_y;

    BlocoInfo *assinaturas = (BlocoInfo *)malloc((size_t)total_blocos * sizeof(BlocoInfo));
    unsigned char *validos = (unsigned char *)calloc((size_t)total_blocos, sizeof(unsigned char));
    unsigned char *usados = (unsigned char *)calloc((size_t)total_blocos, sizeof(unsigned char));
    int *melhor_par = (int *)malloc((size_t)total_blocos * sizeof(int));
    int *melhor_pontuacao = (int *)malloc((size_t)total_blocos * sizeof(int));

    if (!assinaturas || !validos || !usados || !melhor_par || !melhor_pontuacao)
    {
        printf("Erro de memoria\n");
        free(assinaturas);
        free(validos);
        free(usados);
        free(melhor_par);
        free(melhor_pontuacao);
        free(in.pixels);
        free(out.pixels);
        exit(1);
    }

    for (int i = 0; i < total_blocos; i++)
    {
        melhor_par[i] = -1;
        melhor_pontuacao[i] = 1000000;
    }

    for (int gy = 0; gy < grade_y; gy++) {
        for (int gx = 0; gx < grade_x; gx++) {
            int id = gy * grade_x + gx;
            int x = gx * STEP;
            int y = gy * STEP;
            long soma = 0;
            long soma2 = 0;
            int soma_borda = 0;
            int bordas_fortes = 0;
            int quad[4] = {0, 0, 0, 0};

            for (int by = 0; by < BLOCK; by++) {
                for (int bx = 0; bx < BLOCK; bx++) {
                    int xx = x + bx;
                    int yy = y + by;
                    int v = cinza[yy][xx];
                    int borda = bordas[yy][xx];

                    soma += v;
                    soma2 += v * v;
                    soma_borda += borda;
                    if (borda > 24)
                    {
                        bordas_fortes++;
                    }

                    quad[(by >= BLOCK / 2) * 2 + (bx >= BLOCK / 2)] += borda;
                }
            }

            int area = BLOCK * BLOCK;
            int media = (int)(soma / area);
            long variancia = (soma2 / area) - ((long)media * (long)media);
            if (variancia < 0)
            {
                variancia = 0;
            }

            int desvio_padrao = 0;
            while (((long)(desvio_padrao + 1) * (long)(desvio_padrao + 1)) <= variancia)
            {
                desvio_padrao++;
            }

            assinaturas[id].media = media;
            assinaturas[id].desvio_padrao = desvio_padrao;
            assinaturas[id].media_bordas = soma_borda / area;
            assinaturas[id].razao_bordas = (bordas_fortes * 100) / area;

            for (int ty = 0; ty < 4; ty++) {
                for (int tx = 0; tx < 4; tx++) {
                    int soma_celula = 0;
                    int y0 = y + ty * (BLOCK / 4);
                    int x0 = x + tx * (BLOCK / 4);

                    for (int cy = 0; cy < (BLOCK / 4); cy++) {
                        for (int cx = 0; cx < (BLOCK / 4); cx++) {
                            soma_celula += cinza[y0 + cy][x0 + cx];
                        }
                    }

                    int media_celula = soma_celula / ((BLOCK / 4) * (BLOCK / 4));
                    int normalizado = media_celula - media + 128;
                    if (normalizado < 0)
                    {
                        normalizado = 0;
                    }
                    if (normalizado > 255)
                    {
                        normalizado = 255;
                    }

                    assinaturas[id].textura[ty * 4 + tx] = (unsigned char)normalizado;
                }
            }

            for (int q = 0; q < 4; q++)
            {
                int valor_quadrante = quad[q] / ((BLOCK / 2) * (BLOCK / 2));
                if (valor_quadrante > 255)
                {
                    valor_quadrante = 255;
                }
                assinaturas[id].borda_quadrante[q] = (unsigned char)valor_quadrante;
            }

            if (assinaturas[id].desvio_padrao >= MIN_STDDEV && assinaturas[id].media_bordas >= MIN_EDGE_MEAN && assinaturas[id].razao_bordas >= MIN_EDGE_RATIO)
            {
                validos[id] = 1;
            }
        }
    }

    for (int i = 0; i < total_blocos; i++)
    {
        if (!validos[i] || usados[i])
        {
            continue;
        }

        int x1 = (i % grade_x) * STEP;
        int y1 = (i / grade_x) * STEP;

        for (int j = i + 1; j < total_blocos; j++)
        {
            if (!validos[j] || usados[j])
            {
                continue;
            }

            int x2 = (j % grade_x) * STEP;
            int y2 = (j / grade_x) * STEP;
            int dx = x1 - x2;
            int dy = y1 - y2;

            if ((dx * dx + dy * dy) < (MIN_DISTANCE * MIN_DISTANCE))
            {
                continue;
            }

            int score = 0;
            score += abs(assinaturas[i].media - assinaturas[j].media) * 2;
            score += abs(assinaturas[i].desvio_padrao - assinaturas[j].desvio_padrao) * 3;
            score += abs(assinaturas[i].media_bordas - assinaturas[j].media_bordas) * 2;
            score += abs(assinaturas[i].razao_bordas - assinaturas[j].razao_bordas) * 4;

            for (int k=0; k<16; k++)
            {
                score += abs((int)assinaturas[i].textura[k] - (int)assinaturas[j].textura[k]);
            }

            for (int k = 0; k < 4; k++)
            {
                score += abs((int)assinaturas[i].borda_quadrante[k] - (int)assinaturas[j].borda_quadrante[k]);
            }

            score /= 8;

            if (score < melhor_pontuacao[i])
            {
                melhor_pontuacao[i] = score;
                melhor_par[i] = j;
            }

            if (score < melhor_pontuacao[j])
            {
                melhor_pontuacao[j] = score;
                melhor_par[j] = i;
            }
        }
    }

    int pares = 0;

    for (int i = 0; i < total_blocos && pares < MAX_MATCHES; i++)
    {
        if (!validos[i] || usados[i])
        {
            continue;
        }

        int j = melhor_par[i];

        if (melhor_pontuacao[i] >= MATCH_THRESHOLD || melhor_par[j] != i || melhor_pontuacao[j] >= MATCH_THRESHOLD)
        {
            continue;
        }

        int coluna1 = i % grade_x;
        int linha1 = i / grade_x;
        int coluna2 = j % grade_x;
        int linha2 = j / grade_x;
        int deslocamento_x = coluna2 - coluna1;
        int deslocamento_y = linha2 - linha1;
        int apoio = 0;

        for (int dy_viz = -1; dy_viz <= 1; dy_viz++) {
            for (int dx_viz = -1; dx_viz <= 1; dx_viz++) {
                if (dx_viz == 0 && dy_viz == 0)
                {
                    continue;
                }

                int col_viz_i = coluna1 + dx_viz;
                int col_viz_j = coluna1 + dx_viz + deslocamento_x;
                int lin_viz_i = linha1 + dy_viz;
                int lin_viz_j = linha1 + dy_viz + deslocamento_y;

                if (col_viz_i < 0 || col_viz_i >= grade_x || lin_viz_i < 0 || lin_viz_i >= grade_y)
                {
                    continue;
                }

                if (col_viz_j < 0 || col_viz_j >= grade_x || lin_viz_j < 0 || lin_viz_j >= grade_y)
                {
                    continue;
                }

                int indice_i = lin_viz_i * grade_x + col_viz_i;
                int indice_j = lin_viz_j * grade_x + col_viz_j;

                if (!validos[indice_i] || !validos[indice_j])
                {
                    continue;
                }

                if (melhor_par[indice_i] == indice_j && melhor_par[indice_j] == indice_i && melhor_pontuacao[indice_i] < MATCH_THRESHOLD && melhor_pontuacao[indice_j] < MATCH_THRESHOLD)
                {
                    apoio++;
                }
            }
        }

        if (apoio < 3)
        {
            continue;
        }

        int x1 = (i % grade_x) * STEP;
        int y1 = (i / grade_x) * STEP;
        int x2 = (j % grade_x) * STEP;
        int y2 = (j / grade_x) * STEP;

        if (abs(y2 - y1) < STEP)
        {
            continue;
        }

        draw_line(
            out.width,
            out.height,
            pout,
            x1 + BLOCK / 2,
            y1 + BLOCK / 2,
            x2 + BLOCK / 2,
            y2 + BLOCK / 2,
            red,
            2
        );

        usados[i] = 1;
        usados[j] = 1;
        pares++;
    }

    free(assinaturas);
    free(validos);
    free(usados);
    free(melhor_par);
    free(melhor_pontuacao);

    // NÃO ALTERAR A PARTIR DAQUI!

    // Grava a imagem como JPEG para registro
    stbi_write_jpg("saida.jpg", out.width, out.height, 3, pout, 90);

    free(in.pixels);
    free(out.pixels);
}

void load(char *name, Img *pic)
{
    pic->pixels = (Pixel *)stbi_load(name, &pic->width, &pic->height, &pic->channels, 0);
    if (!pic->pixels)
    {
        printf("STB loading error\n");
        exit(1);
    }
    printf("Load: %d x %d x %d\n", pic->width, pic->height, pic->channels);
    // Exibe os 16 primeiros pixels (teste)
    for (int i = 0; i < 16; i++)
    {
        printf("[%02X %02X %02X] ", pic->pixels[i].r, pic->pixels[i].g, pic->pixels[i].b);
    }
    printf("\n");
}

// Algoritmo de Bresenham para desenhar uma linha em uma matriz de pixels
void draw_line(int width, int height, Pixel img[][width], int x0, int y0, int x1, int y1, Pixel color, int thickness) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    int half = thickness / 2;
    while (1) {
        // Draw a square of size thickness x thickness centered at (x0, y0)
        for (int i = -half; i <= half; i++) {
            for (int j = -half; j <= half; j++) {
                int xi = x0 + i, yj = y0 + j;
                if (xi >= 0 && xi < width && yj >= 0 && yj < height)
                    img[yj][xi] = color;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)  {
            err += dx;
            y0 += sy;
        }
    }
}

