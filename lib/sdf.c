#include <stdio.h>
#include <stdlib.h>

#include <graphics.h>
#include <hashmap.h>
#include <sdf.h>

#include <decodeutf8.h>
#include <ununicode.h>
#define FONT_COUNT 1

#define BASE_WIDTH 50
#define BASE_HEIGHT 50

static sprite_t _font_data_thin;
static sprite_t _font_data_bold;
static sprite_t _font_data_oblique;
static sprite_t _font_data_bold_oblique;
static sprite_t _font_data_mono;
static sprite_t _font_data_mono_bold;
static sprite_t _font_data_mono_oblique;
static sprite_t _font_data_mono_bold_oblique;

static hashmap_t * _font_cache;

static volatile int _sdf_lock = 0;
static double gamma = 1.7;

struct CharData{
	char code;
	size_t width_bold;
	size_t width_thin;
	size_t width_mono;
} _char_data[256];

static int loaded = 0;

static int offset(int ch) {
	/* Calculate offset into table above */
	return ch;
}

static char * _font_data = NULL;
static size_t _font_data_size = 0;
static sprite_t _font_sprites[FONT_COUNT];

static void init_fonts();

static void load_font(sprite_t * sprite, int font) {
	uint32_t * _font_data_i = (uint32_t*)_font_data;

	sprite->width = _font_data_i[font * 3 + 1];
	sprite->height = _font_data_i[font * 3 + 2];

	int offset = _font_data_i[font * 3 + 3];
	sprite->bitmap = (uint32_t *)&_font_data[offset];
	sprite->alpha = 0;
	sprite->masks = NULL;
	sprite->blank = 0;
    printf(1, "finish load_font\n");
}

void init_sdf(void) {
	/* Load the font. */
    init_fonts();
	_font_cache = hashmap_create_int(10);
    /*
	{
		char tmp[100];
		char * display = getenv("DISPLAY");
		if (!display) display = "compositor";
		sprintf(tmp, "sys.%s.fonts", display);
		_font_data = shm_obtain(tmp, &_font_data_size);
	}
    */

    _font_data_size = sizeof(unsigned int) * (1 + FONT_COUNT * 3);
    for (int i = 0; i < FONT_COUNT; ++i) {
        printf(1, "bitmap size {%d, %d}\n", _font_sprites[0].width, _font_sprites[0].height);
        _font_data_size += 4 * _font_sprites[i].width * _font_sprites[i].height;
    }

    printf(1, "font size = %d \n", _font_data_size);

    _font_data = malloc(sizeof(char) * _font_data_size);
    char *font = _font_data;
    uint32_t * data = (uint32_t *)_font_data;

    printf(1, "data malloc\n");

    data[0] = FONT_COUNT;
    data[1] = _font_sprites[0].width;
    data[2] = _font_sprites[0].height;
    data[3] = (FONT_COUNT * 3 + 1) * sizeof(unsigned int);
    memcpy(&font[data[3]], _font_sprites[0].bitmap, _font_sprites[0].width * _font_sprites[0].height * 4);
    free(_font_sprites[0].bitmap);

    printf(1, "data finish\n");

    for (int i = 1; i < FONT_COUNT; ++i) {
        printf(1, "  Loaded %d font(s)... %d %d %d", i, data[(i - 1) * 3 + 2], data[(i - 1) * 3 + 1], data[(i - 1) * 3 + 3]);
        data[i * 3 + 1] = _font_sprites[i].width;
        data[i * 3 + 2] = _font_sprites[i].height;
        data[i * 3 + 3] = data[(i - 1) * 3 + 3] + data[(i - 1) * 3 + 2] * data[(i - 1) * 3 + 1] * 4;
        memcpy(&font[data[i * 3 + 3]], _font_sprites[i].bitmap, _font_sprites[i].width * _font_sprites[i].height * 4);
        free(_font_sprites[i].bitmap);
    }

	load_font(&_font_data_thin, SDF_FONT_THIN);
    /*
	load_font(&_font_data_bold, SDF_FONT_BOLD);
	load_font(&_font_data_oblique, SDF_FONT_OBLIQUE);
	load_font(&_font_data_bold_oblique, SDF_FONT_BOLD_OBLIQUE);
	load_font(&_font_data_mono, SDF_FONT_MONO);
	load_font(&_font_data_mono_bold, SDF_FONT_MONO_BOLD);
	load_font(&_font_data_mono_oblique, SDF_FONT_MONO_OBLIQUE);
	load_font(&_font_data_mono_bold_oblique, SDF_FONT_MONO_BOLD_OBLIQUE);
    */

    /*
	FILE * fi = fopen("/etc/sdf.conf", "r");
	char tmp[1024];
	char * s = tmp;
    */

    for (int i = 0; i < 256; ++i) {
		_char_data[i].code = i;
		_char_data[i].width_bold = 25;
		_char_data[i].width_thin = 25;
		_char_data[i].width_mono = 25;
	}
    /*
	while ((s = fgets(tmp, 1024, fi))) {
		if (strlen(s) < 1) continue;
		int i = offset(*s);
		s++; s++;
		char t = *s;
		s++; s++;
		int o = atoi(s);
		if (t == 'b') {
			_char_data[i].width_bold = o;
		} else if (t == 't') {
			_char_data[i].width_thin = o;
		} else if (t == 'm') {
			_char_data[i].width_mono = o;
		}
	}
	fclose(fi);
    */
	loaded = 1;
}

static sprite_t * _select_font(int font) {
	switch (font) {
		case SDF_FONT_BOLD:
			return &_font_data_bold;
		case SDF_FONT_MONO:
			return &_font_data_mono;
		case SDF_FONT_MONO_BOLD:
			return &_font_data_mono_bold;
		case SDF_FONT_MONO_OBLIQUE:
			return &_font_data_mono_oblique;
		case SDF_FONT_MONO_BOLD_OBLIQUE:
			return &_font_data_mono_bold_oblique;
		case SDF_FONT_OBLIQUE:
			return &_font_data_oblique;
		case SDF_FONT_BOLD_OBLIQUE:
			return &_font_data_bold_oblique;
		case SDF_FONT_THIN:
		default:
			return &_font_data_thin;
	}
}

static int _select_width(int _ch, int font) {
	int ch = (_ch >= 0 && _ch <= 128) ? _ch : (int)ununicode(_ch);
	switch (font) {
		case SDF_FONT_BOLD:
		case SDF_FONT_BOLD_OBLIQUE:
			return _char_data[ch].width_bold;
		case SDF_FONT_MONO:
		case SDF_FONT_MONO_BOLD:
		case SDF_FONT_MONO_OBLIQUE:
		case SDF_FONT_MONO_BOLD_OBLIQUE:
			return _char_data[ch].width_mono;
		case SDF_FONT_OBLIQUE:
		case SDF_FONT_THIN:
		default:
			return _char_data[ch].width_thin;
	}
}

static int draw_sdf_character(gfx_context_t * ctx, int32_t x, int32_t y, int _ch, int size, uint32_t color, sprite_t * tmp, int font, sprite_t * _font_data, double buffer) {
	int ch = (_ch >= 0 && _ch <= 128) ? _ch : (int)ununicode(_ch);

	double scale = (double)size / 50.0;
	int width = _select_width(ch, font) * scale;
	int fx = ((BASE_WIDTH * ch) % _font_data->width) * scale;
	int fy = (((BASE_WIDTH * ch) / _font_data->width) * BASE_HEIGHT) * scale;

	int height = BASE_HEIGHT * ((double)size / 50.0);


	/* ignore size */
	for (int j = 0; j < height; ++j) {
		if (y + j < 0) continue;
		if (y + j >= ctx->height) continue;
		if (fy+j >= tmp->height) continue;
		for (int i = 0; i < size; ++i) {
			/* TODO needs to do bilinear filter */
			if (fx+i >= tmp->width) continue;
			if (x + i < 0) continue;
			if (x + i >= ctx->width) continue;
			uint32_t c = SPRITE((tmp), fx+i, fy+j);
			double dist = (double)_RED(c) / 255.0;
			double edge0 = buffer - gamma * 1.4142 / (double)size;
			double edge1 = buffer + gamma * 1.4142 / (double)size;
			double a = (dist - edge0) / (edge1 - edge0);
			if (a < 0.0) a = 0.0;
			if (a > 1.0) a = 1.0;
			a = a * a * (3 - 2 * a);
			uint32_t f_color = premultiply((color & 0xFFFFFF) | ((uint32_t)(255 * a) << 24));
			f_color = (f_color & 0xFFFFFF) | ((uint32_t)(a * _ALP(color)) << 24);
			GFX(ctx,x+i,y+j) = alpha_blend_rgba(GFX(ctx,x+i,y+j), f_color);
		}
	}

	return width;

}

int draw_sdf_string_stroke(gfx_context_t * ctx, int32_t x, int32_t y, const char * str, int size, uint32_t color, int font, double _gamma, double stroke) {

	sprite_t * _font_data = _select_font(font);

    printf(1, "%x \n", _font_data);

	if (!loaded) {
        printf(1, ">>>>>>>>>not init sdf.\n");
        return 0;
    }

    printf(1, ">>>>>>>>>loaded\n");

	double scale = (double)size / 50.0;
	int scale_height = scale * _font_data->height;

	sprite_t * tmp;
	//spin_lock(&_sdf_lock);
	if (!hashmap_has(_font_cache, (void *)(uintptr_t)(scale_height | (font << 16)))) {
        printf(1, ">>>>>>>>>no hash \n");
		tmp = create_sprite(scale * _font_data->width, scale * _font_data->height, ALPHA_OPAQUE);
		gfx_context_t * t = init_graphics_sprite(tmp);
		draw_sprite_scaled(t, _font_data, 0, 0, tmp->width, tmp->height);
		free(t);
		hashmap_set(_font_cache, (void *)(uintptr_t)(scale_height | (font << 16)), tmp);
	} else {
        printf(1, ">>>>>>>>> else \n");
		tmp = hashmap_get(_font_cache, (void *)(uintptr_t)(scale_height | (font << 16)));
	}
    printf(1, ">>>>>>>>> tmp \n");

	uint32_t state = 0;
	uint32_t c = 0;
	int32_t out_width = 0;
	gamma = _gamma;
	while (*str) {
		if (!decode(&state, &c, (unsigned char)*str)) {
			int w = draw_sdf_character(ctx,x,y,c,size,color,tmp,font,_font_data, stroke);
			out_width += w;
			x += w;
		}
		str++;
	}
	//spin_unlock(&_sdf_lock);

	return out_width;
}

int draw_sdf_string_gamma(gfx_context_t * ctx, int32_t x, int32_t y, const char * str, int size, uint32_t color, int font, double _gamma) {
	return draw_sdf_string_stroke(ctx,x,y,str,size,color,font,gamma,0.75);
}

int draw_sdf_string(gfx_context_t * ctx, int32_t x, int32_t y, const char * str, int size, uint32_t color, int font) {
	return draw_sdf_string_stroke(ctx,x,y,str,size,color,font,1.7, 0.75);
}

static int char_width(int ch, int font) {
	return _select_width(ch, font);
}


int draw_sdf_string_width(const char * str, int size, int font) {
	double scale = (double)size / 50.0;

	uint32_t state = 0;
	uint32_t c = 0;

	int32_t out_width = 0;
	while (*str) {
		if (!decode(&state, &c, (unsigned char)*str)) {
			int w = char_width(c,font) * scale;
			out_width += w;
		} else if (state == UTF8_REJECT) {
			state = 0;
		}
		str++;
	}

	return out_width;
}

static void init_fonts() {
    printf(1, "init_fonts \n");
    load_sprite(&_font_sprites[0], "/usr/share/fonts/sdf_thin.sdf");
    /*
    load_sprite(&_font_sprites[1], "/usr/share/fonts/sdf_bold.sdf");
    load_sprite(&_font_sprites[2], "/usr/share/fonts/sdf_mono.sdf");
    load_sprite(&_font_sprites[3], "/usr/share/fonts/sdf_mono_bold.sdf");
    load_sprite(&_font_sprites[4], "/usr/share/fonts/sdf_mono_oblique.sdf");
    load_sprite(&_font_sprites[5], "/usr/share/fonts/sdf_mono_bold_oblique.sdf");
    load_sprite(&_font_sprites[6], "/usr/share/fonts/sdf_oblique.sdf");
    load_sprite(&_font_sprites[7], "/usr/share/fonts/sdf_bold_oblique.sdf");
    */
}

