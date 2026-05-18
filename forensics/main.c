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
    
    // =====================================================
    // Algoritmo de detecção de clones (Copy-Move Forgery)
    // =====================================================

    const int BLOCK = 16;
    const int STEP = 8;
    const int MIN_DISTANCE = 48;
    const int MAX_MATCHES = 18;
    const int MIN_STDDEV = 18;
    const int MIN_EDGE_MEAN = 22;
    const int MIN_EDGE_RATIO = 10;
    const int MATCH_THRESHOLD = 220;

    typedef struct
    {
        int mean;
        int stddev;
        int edge_mean;
        int edge_ratio;
        unsigned char texture[16];
        unsigned char edge_quad[4];
    } BlockSig;

    // Copia imagem original para saída
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {
            unsigned char lum =
                (unsigned char)(
                    0.59 * pin[y][x].g +
                    0.30 * pin[y][x].r +
                    0.11 * pin[y][x].b
                );

            pout[y][x].r = lum;
            pout[y][x].g = lum;
            pout[y][x].b = lum;
        }
    }

    // Matriz de luminância
    unsigned char gray[in.height][in.width];

    // Converte RGB para tons de cinza
    for (int y = 0; y < in.height; y++) {
        for (int x = 0; x < in.width; x++) {

            Pixel p = pin[y][x];

            gray[y][x] =
                (unsigned char)(
                    0.59 * p.g +
                    0.30 * p.r +
                    0.11 * p.b
                );
        }
    }

    unsigned char edges[in.height][in.width];

    for (int y = 1; y < in.height - 1; y++) {
        for (int x = 1; x < in.width - 1; x++) {

            int gx =
                -gray[y-1][x-1] + gray[y-1][x+1]
                -2*gray[y][x-1] + 2*gray[y][x+1]
                -gray[y+1][x-1] + gray[y+1][x+1];

            int gy =
                -gray[y-1][x-1] -2*gray[y-1][x] -gray[y-1][x+1]
                +gray[y+1][x-1] +2*gray[y+1][x] +gray[y+1][x+1];

            int mag = abs(gx) + abs(gy);

            if (mag > 255)
                mag = 255;

            edges[y][x] = mag;
        }
    }

    Pixel red = {255, 0, 0};

    int grid_x = ((in.width - BLOCK) / STEP) + 1;
    int grid_y = ((in.height - BLOCK) / STEP) + 1;
    int total_blocks = grid_x * grid_y;

    if (grid_x <= 0 || grid_y <= 0)
    {
        printf("Imagem muito pequena para analise\n");
        free(in.pixels);
        free(out.pixels);
        exit(1);
    }

    BlockSig *sigs = (BlockSig *)malloc((size_t)total_blocks * sizeof(BlockSig));
    unsigned char *valid = (unsigned char *)calloc((size_t)total_blocks, sizeof(unsigned char));
    unsigned char *used = (unsigned char *)calloc((size_t)total_blocks, sizeof(unsigned char));
    int *best_match = (int *)malloc((size_t)total_blocks * sizeof(int));
    int *best_score = (int *)malloc((size_t)total_blocks * sizeof(int));

    if (!sigs || !valid || !used || !best_match || !best_score)
    {
        printf("Erro de memoria\n");
        free(sigs);
        free(valid);
        free(used);
        free(best_match);
        free(best_score);
        free(in.pixels);
        free(out.pixels);
        exit(1);
    }

    for (int i = 0; i < total_blocks; i++)
    {
        best_match[i] = -1;
        best_score[i] = 1000000;
    }

    // Monta assinaturas dos blocos usando grayscale + textura + bordas
    for (int gy = 0; gy < grid_y; gy++)
    {
        for (int gx = 0; gx < grid_x; gx++)
        {
            int idx = gy * grid_x + gx;
            int x = gx * STEP;
            int y = gy * STEP;
            long sum = 0;
            long sum_sq = 0;
            int edge_sum = 0;
            int edge_hits = 0;
            int quad_sum[4] = {0, 0, 0, 0};

            for (int by = 0; by < BLOCK; by++)
            {
                for (int bx = 0; bx < BLOCK; bx++)
                {
                    int px = x + bx;
                    int py = y + by;
                    int lum = gray[py][px];
                    int edge_val = edges[py][px];

                    sum += lum;
                    sum_sq += lum * lum;
                    edge_sum += edge_val;
                    if (edge_val > 24)
                    {
                        edge_hits++;
                    }

                    quad_sum[(by >= BLOCK / 2) * 2 + (bx >= BLOCK / 2)] += edge_val;
                }
            }

            int area = BLOCK * BLOCK;
            int mean = (int)(sum / area);
            long variance = (sum_sq / area) - ((long)mean * (long)mean);
            if (variance < 0)
            {
                variance = 0;
            }

            int stddev = 0;
            while (((long)(stddev + 1) * (long)(stddev + 1)) <= variance)
            {
                stddev++;
            }

            sigs[idx].mean = mean;
            sigs[idx].stddev = stddev;
            sigs[idx].edge_mean = edge_sum / area;
            sigs[idx].edge_ratio = (edge_hits * 100) / area;

            for (int ty = 0; ty < 4; ty++)
            {
                for (int tx = 0; tx < 4; tx++)
                {
                    int cell_sum = 0;
                    int cell_y0 = y + ty * (BLOCK / 4);
                    int cell_x0 = x + tx * (BLOCK / 4);

                    for (int cy = 0; cy < (BLOCK / 4); cy++)
                    {
                        for (int cx = 0; cx < (BLOCK / 4); cx++)
                        {
                            cell_sum += gray[cell_y0 + cy][cell_x0 + cx];
                        }
                    }

                    int cell_mean = cell_sum / ((BLOCK / 4) * (BLOCK / 4));
                    int normalized = cell_mean - mean + 128;
                    if (normalized < 0)
                    {
                        normalized = 0;
                    }
                    if (normalized > 255)
                    {
                        normalized = 255;
                    }

                    sigs[idx].texture[ty * 4 + tx] = (unsigned char)normalized;
                }
            }

            for (int q = 0; q < 4; q++)
            {
                int quad_value = quad_sum[q] / ((BLOCK / 2) * (BLOCK / 2));
                if (quad_value > 255)
                {
                    quad_value = 255;
                }
                sigs[idx].edge_quad[q] = (unsigned char)quad_value;
            }

            if (sigs[idx].stddev >= MIN_STDDEV && sigs[idx].edge_mean >= MIN_EDGE_MEAN && sigs[idx].edge_ratio >= MIN_EDGE_RATIO)
            {
                valid[idx] = 1;
            }
        }
    }

    // Descobre o melhor parceiro de cada bloco válido
    for (int i = 0; i < total_blocks; i++)
    {
        if (!valid[i] || used[i])
        {
            continue;
        }

        int x1 = (i % grid_x) * STEP;
        int y1 = (i / grid_x) * STEP;

        for (int j = i + 1; j < total_blocks; j++)
        {
            if (!valid[j] || used[j])
            {
                continue;
            }

            int x2 = (j % grid_x) * STEP;
            int y2 = (j / grid_x) * STEP;
            int dx = x1 - x2;
            int dy = y1 - y2;

            // Ignora regiões muito próximas
            if ((dx * dx + dy * dy) < (MIN_DISTANCE * MIN_DISTANCE))
            {
                continue;
            }

            int score = 0;
            score += abs(sigs[i].mean - sigs[j].mean) * 2;
            score += abs(sigs[i].stddev - sigs[j].stddev) * 3;
            score += abs(sigs[i].edge_mean - sigs[j].edge_mean) * 2;
            score += abs(sigs[i].edge_ratio - sigs[j].edge_ratio) * 4;

            for (int k = 0; k < 16; k++)
            {
                score += abs((int)sigs[i].texture[k] - (int)sigs[j].texture[k]);
            }

            for (int k = 0; k < 4; k++)
            {
                score += abs((int)sigs[i].edge_quad[k] - (int)sigs[j].edge_quad[k]);
            }

            score /= 8;

            if (score < best_score[i])
            {
                best_score[i] = score;
                best_match[i] = j;
            }

            if (score < best_score[j])
            {
                best_score[j] = score;
                best_match[j] = i;
            }
        }
    }

    int matches = 0;

    // Desenha apenas pares recíprocos e distintos
    for (int i = 0; i < total_blocks && matches < MAX_MATCHES; i++)
    {
        if (!valid[i] || used[i])
        {
            continue;
        }

        int j = best_match[i];
        if (j < 0 || j >= total_blocks)
        {
            continue;
        }

        if (best_score[i] >= MATCH_THRESHOLD || best_match[j] != i || best_score[j] >= MATCH_THRESHOLD)
        {
            continue;
        }

        int gx1 = i % grid_x;
        int gy1 = i / grid_x;
        int gx2 = j % grid_x;
        int gy2 = j / grid_x;
        int shift_x = gx2 - gx1;
        int shift_y = gy2 - gy1;
        int support = 0;

        // Confirma se a vizinhanca ao redor do bloco também segue o mesmo deslocamento
        for (int oy = -1; oy <= 1; oy++)
        {
            for (int ox = -1; ox <= 1; ox++)
            {
                if (ox == 0 && oy == 0)
                {
                    continue;
                }

                int ngi = gx1 + ox;
                int ngj = gx1 + ox + shift_x;
                int ngyi = gy1 + oy;
                int ngyj = gy1 + oy + shift_y;

                if (ngi < 0 || ngi >= grid_x || ngyi < 0 || ngyi >= grid_y)
                {
                    continue;
                }

                if (ngj < 0 || ngj >= grid_x || ngyj < 0 || ngyj >= grid_y)
                {
                    continue;
                }

                int ni = ngyi * grid_x + ngi;
                int nj = ngyj * grid_x + ngj;

                if (!valid[ni] || !valid[nj])
                {
                    continue;
                }

                if (best_match[ni] == nj && best_match[nj] == ni && best_score[ni] < MATCH_THRESHOLD && best_score[nj] < MATCH_THRESHOLD)
                {
                    support++;
                }
            }
        }

        if (support < 3)
        {
            continue;
        }

        int x1 = (i % grid_x) * STEP;
        int y1 = (i / grid_x) * STEP;
        int x2 = (j % grid_x) * STEP;
        int y2 = (j / grid_x) * STEP;

        // Evita as linhas horizontais espurias que ainda aparecem em faixas repetidas
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

        used[i] = 1;
        used[j] = 1;
        matches++;
    }

    free(sigs);
    free(valid);
    free(used);
    free(best_match);
    free(best_score);

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

