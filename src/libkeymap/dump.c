/* loadkeys.parse.y
 *
 * This file is part of kbd project.
 * Copyright (C) 1993  Risto Kankkunen.
 * Copyright (C) 1993  Eugene G. Crosser.
 * Copyright (C) 1994-2007  Andries E. Brouwer.
 * Copyright (C) 2007-2012  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "ksyms.h"
#include "modifiers.h"
#include "nls.h"

#include "keymap.h"
#include "parseP.h"

int
lk_dump_bkeymap(struct keymap *kmap)
{
	int i, j;

	//u_char *p;
	char flag, magic[] = "bkeymap";
	unsigned short v;

	if (do_constant(kmap) < 0)
		return -1;

	if (write(1, magic, 7) == -1)
		goto fail;
	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		flag = kmap->key_map[i] ? 1 : 0;
		if (write(1, &flag, 1) == -1)
			goto fail;
	}
	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (kmap->key_map[i]) {
			for (j = 0; j < NR_KEYS / 2; j++) {
				v = kmap->key_map[i][j];
				if (write(1, &v, 2) == -1)
					goto fail;
			}
		}
	}
	return 0;

 fail:	log_error(kmap, _("Error writing map to file"));
	return -1;
}

int
lk_dump_ctable(struct keymap *kmap, FILE *fd)
{
	int j;
	unsigned int i, imax;

	char *ptr;
	unsigned int maxfunc;
	unsigned int keymap_count = 0;
	unsigned int func_table_offs[MAX_NR_FUNC];
	unsigned int func_buf_offset = 0;

	if (do_constant(kmap) < 0)
		return -1;

	fprintf(fd,
/* not to be translated... */
		    "/* Do not edit this file! It was automatically generated by   */\n");
	fprintf(fd, "/*    loadkeys --mktable defkeymap.map > defkeymap.c          */\n\n");
	fprintf(fd, "#include <linux/types.h>\n");
	fprintf(fd, "#include <linux/keyboard.h>\n");
	fprintf(fd, "#include <linux/kd.h>\n\n");

	for (i = 0; i < MAX_NR_KEYMAPS; i++)
		if (kmap->key_map[i]) {
			keymap_count++;
			if (i)
				fprintf(fd, "static ");
			fprintf(fd, "u_short %s_map[NR_KEYS] = {", mk_mapname(i));
			for (j = 0; j < NR_KEYS; j++) {
				if (!(j % 8))
					fprintf(fd, "\n");
				fprintf(fd, "\t0x%04x,", U((kmap->key_map[i])[j]));
			}
			fprintf(fd, "\n};\n\n");
		}

	for (imax = MAX_NR_KEYMAPS - 1; imax > 0; imax--)
		if (kmap->key_map[imax])
			break;
	fprintf(fd, "ushort *key_maps[MAX_NR_KEYMAPS] = {");
	for (i = 0; i <= imax; i++) {
		fprintf(fd, (i % 4) ? " " : "\n\t");
		if (kmap->key_map[i])
			fprintf(fd, "%s_map,", mk_mapname(i));
		else
			fprintf(fd, "0,");
	}
	if (imax < MAX_NR_KEYMAPS - 1)
		fprintf(fd, "\t0");
	fprintf(fd, "\n};\n\nunsigned int keymap_count = %d;\n\n", keymap_count);

/* uglified just for xgettext - it complains about nonterminated strings */
	fprintf(fd,
	       "/*\n"
	       " * Philosophy: most people do not define more strings, but they who do\n"
	       " * often want quite a lot of string space. So, we statically allocate\n"
	       " * the default and allocate dynamically in chunks of 512 bytes.\n"
	       " */\n" "\n");
	for (maxfunc = MAX_NR_FUNC; maxfunc; maxfunc--)
		if (kmap->func_table[maxfunc - 1])
			break;

	fprintf(fd, "char func_buf[] = {\n");
	for (i = 0; i < maxfunc; i++) {
		ptr = kmap->func_table[i];
		if (ptr) {
			func_table_offs[i] = func_buf_offset;
			fprintf(fd, "\t");
			for (; *ptr; ptr++)
				outchar(fd, *ptr, 1);
			fprintf(fd, "0, \n");
			func_buf_offset += (ptr - kmap->func_table[i] + 1);
		}
	}
	if (!maxfunc)
		fprintf(fd, "\t0\n");
	fprintf(fd, "};\n\n");

	fprintf(fd,
	       "char *funcbufptr = func_buf;\n"
	       "int funcbufsize = sizeof(func_buf);\n"
	       "int funcbufleft = 0;          /* space left */\n" "\n");

	fprintf(fd, "char *func_table[MAX_NR_FUNC] = {\n");
	for (i = 0; i < maxfunc; i++) {
		if (kmap->func_table[i])
			fprintf(fd, "\tfunc_buf + %u,\n", func_table_offs[i]);
		else
			fprintf(fd, "\t0,\n");
	}
	if (maxfunc < MAX_NR_FUNC)
		fprintf(fd, "\t0,\n");
	fprintf(fd, "};\n");

#ifdef KDSKBDIACRUC
	if (kmap->prefer_unicode) {
		fprintf(fd, "\nstruct kbdiacruc accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < kmap->accent_table_size; i++) {
			fprintf(fd, "\t{");
			outchar(fd, kmap->accent_table[i].diacr, 1);
			outchar(fd, kmap->accent_table[i].base, 1);
			fprintf(fd, "0x%04x},", kmap->accent_table[i].result);
			if (i % 2)
				fprintf(fd, "\n");
		}
		if (i % 2)
			fprintf(fd, "\n");
		fprintf(fd, "};\n\n");
	} else
#endif
	{
		fprintf(fd, "\nstruct kbdiacr accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < kmap->accent_table_size; i++) {
			fprintf(fd, "\t{");
			outchar(fd, kmap->accent_table[i].diacr, 1);
			outchar(fd, kmap->accent_table[i].base, 1);
			outchar(fd, kmap->accent_table[i].result, 0);
			fprintf(fd, "},");
			if (i % 2)
				fprintf(fd, "\n");
		}
		if (i % 2)
			fprintf(fd, "\n");
		fprintf(fd, "};\n\n");
	}
	fprintf(fd, "unsigned int accent_table_size = %d;\n", kmap->accent_table_size);
	return 0;
}

/* void dump_funcs(void) */
void
lk_dump_funcs(struct keymap *kmap, FILE *fd)
{
	unsigned int i;
	char *ptr;
	unsigned int maxfunc;

	for (maxfunc = MAX_NR_FUNC; maxfunc; maxfunc--) {
		if (kmap->func_table[maxfunc - 1])
			break;
	}

	for (i = 0; i < maxfunc; i++) {
		ptr = kmap->func_table[i];
		if (!ptr)
			continue;

		fprintf(fd, "string %s = \"", syms[KT_FN].table[i]);

		for (; *ptr; ptr++) {
			if (*ptr == '"' || *ptr == '\\') {
				fputc('\\', fd);
				fputc(*ptr, fd);
			} else if (isgraph(*ptr) || *ptr == ' ') {
				fputc(*ptr, fd);
			} else {
				fprintf(fd, "\\%03o", *ptr);
			}
		}
		fputc('"', fd);
		fputc('\n', fd);
	}
}

/* void dump_diacs(void) */
void
lk_dump_diacs(struct keymap *kmap, FILE *fd)
{
	unsigned int i;
#ifdef KDSKBDIACRUC
	if (kmap->prefer_unicode) {
		for (i = 0; i < kmap->accent_table_size; i++) {
			fprintf(fd, "compose ");
			dumpchar(fd, kmap->accent_table[i].diacr & 0xff, 0);
			fprintf(fd, " ");
			dumpchar(fd, kmap->accent_table[i].base & 0xff, 0);
			fprintf(fd, " to U+%04x\n", kmap->accent_table[i].result);
		}
	} else
#endif
	{
		for (i = 0; i < kmap->accent_table_size; i++) {
			fprintf(fd, "compose ");
			dumpchar(fd, kmap->accent_table[i].diacr, 0);
			fprintf(fd, " ");
			dumpchar(fd, kmap->accent_table[i].base, 0);
			fprintf(fd, " to ");
			dumpchar(fd, kmap->accent_table[i].result, 0);
			fprintf(fd, "\n");
		}
	}
}

void
lk_dump_keymaps(struct keymap *kmap, FILE *fd)
{
	int i, m0, m;
	char c = ' ';

	fprintf(fd, "keymaps");
	for (i = 0; i < kmap->max_keymap; i++) {
		if (i)
			c = ',';
		m0 = m = kmap->defining[i];
		while (i+1 < kmap->max_keymap && kmap->defining[i+1] == m+1)
			i++, m++;
		if (!m0)
			continue;
		(m0 == m)
			? fprintf(fd, "%c%d"   , c, m0-1)
			: fprintf(fd, "%c%d-%d", c, m0-1, m-1);
	}
	fprintf(fd, "\n");
}

static void
print_mod(FILE *fd, int x)
{
	if (x) {
		modifier_t *mod = (modifier_t *) modifiers;
		while (mod->name) {
			if (x & (1 << mod->bit))
				fprintf(fd, "%s\t", mod->name);
			mod++;
		}
	} else {
		fprintf(fd, "plain\t");
	}
}

static void
print_keysym(struct keymap *kmap, FILE *fd, int code, char numeric)
{
	unsigned int t;
	int v;
	const char *p;
	int plus;

	fprintf(fd, " ");
	t = KTYP(code);
	v = KVAL(code);
	if (t >= syms_size) {
		if (!numeric && (p = codetoksym(kmap, code)) != NULL)
			fprintf(fd, "%-16s", p);
		else
			fprintf(fd, "U+%04x          ", code ^ 0xf000);
		return;
	}
	plus = 0;
	if (t == KT_LETTER) {
		t = KT_LATIN;
		fprintf(fd, "+");
		plus++;
	}
	if (!numeric && t < syms_size && v < syms[t].size &&
	    (p = syms[t].table[v])[0])
		fprintf(fd, "%-*s", 16 - plus, p);
	else if (!numeric && t == KT_META && v < 128 && v < syms[0].size &&
		 (p = syms[0].table[v])[0])
		fprintf(fd, "Meta_%-11s", p);
	else
		fprintf(fd, "0x%04x         %s", code, plus ? "" : " ");
}

static void
print_bind(struct keymap *kmap, FILE *fd, int bufj, int i, int j, char numeric)
{
	if(j)
		fprintf(fd, "\t");
	print_mod(fd, j);
	fprintf(fd, "keycode %3d =", i);
	print_keysym(kmap, fd, bufj, numeric);
	fprintf(fd, "\n");
}

void
lk_dump_keys(struct keymap *kmap, FILE *fd, char table_shape, char numeric)
{
	int i, j, k;
	int buf[MAX_NR_KEYMAPS];
	int isletter, islatin, isasexpected;
	int typ, val;
	int alt_is_meta = 0;
	int all_holes;
	int zapped[MAX_NR_KEYMAPS];
	int keymapnr = kmap->max_keymap;

	if (!keymapnr)
		return;

	if (table_shape == FULL_TABLE || table_shape == SEPARATE_LINES)
		goto no_shorthands;

	/* first pass: determine whether to set alt_is_meta */
	for (j = 0; j < MAX_NR_KEYMAPS; j++) {
		int ja = (j | M_ALT);

		if (!(j != ja && kmap->defining[j] && kmap->defining[ja]))
			continue;

		for (i = 1; i < NR_KEYS; i++) {
			int buf0, buf1, type;

			buf0 = (kmap->key_map[j])[i];

			if (buf0 == -1)
				break;

			type = KTYP(buf0);

			if ((type == KT_LATIN || type == KT_LETTER) && KVAL(buf0) < 128) {
				buf1 = (kmap->defining[ja])
					? kmap->key_map[ja][i]
					: -1;

				if (buf1 != K(KT_META, KVAL(buf0)))
					goto not_alt_is_meta;
			}
		}
	}
	alt_is_meta = 1;
	fprintf(fd, "alt_is_meta\n");

not_alt_is_meta:
no_shorthands:

	for (i = 1; i < NR_KEYS; i++) {
		all_holes = 1;

		for (j = 0; j < keymapnr; j++) {
			buf[j] = (kmap->defining[j])
				? (kmap->key_map[(kmap->defining[j])-1])[i]
				: K_HOLE;

			if (buf[j] != K_HOLE)
				all_holes = 0;
		}

		if (all_holes)
			continue;

		if (table_shape == FULL_TABLE) {
			fprintf(fd, "keycode %3d =", i);

			for (j = 0; j < keymapnr; j++)
				print_keysym(kmap, fd, buf[j], numeric);

			fprintf(fd, "\n");
			continue;
		}

		if (table_shape == SEPARATE_LINES) {
			for (j = 0; j < keymapnr; j++) {
				//if (buf[j] != K_HOLE)
				print_bind(kmap, fd, buf[j], i, kmap->defining[j]-1, numeric);
			}

			fprintf(fd, "\n");
			continue;
		}

		fprintf(fd, "\n");

		typ = KTYP(buf[0]);
		val = KVAL(buf[0]);
		islatin = (typ == KT_LATIN || typ == KT_LETTER);
		isletter = (islatin &&
			((val >= 'A' && val <= 'Z') ||
			 (val >= 'a' && val <= 'z')));

		isasexpected = 0;
		if (isletter) {
			u_short defs[16];
			defs[0] = K(KT_LETTER, val);
			defs[1] = K(KT_LETTER, val ^ 32);
			defs[2] = defs[0];
			defs[3] = defs[1];

			for (j = 4; j < 8; j++)
				defs[j] = K(KT_LATIN, val & ~96);

			for (j = 8; j < 16; j++)
				defs[j] = K(KT_META, KVAL(defs[j-8]));

			for (j = 0; j < keymapnr; j++) {
				k = kmap->defining[j] - 1;

				if ((k >= 16 && buf[j] != K_HOLE) || (k < 16 && buf[j] != defs[k]))
					goto unexpected;
			}

			isasexpected = 1;
		}
unexpected:

		/* wipe out predictable meta bindings */
		for (j = 0; j < keymapnr; j++)
			zapped[j] = 0;

		if (alt_is_meta) {
			for(j = 0; j < keymapnr; j++) {
				int ka, ja, ktyp;

				k = kmap->defining[j] - 1;
				ka = (k | M_ALT);
				ja = kmap->defining[ka] - 1;

				if (k != ka && ja >= 0
				    && ((ktyp=KTYP(buf[j])) == KT_LATIN || ktyp == KT_LETTER)
				    && KVAL(buf[j]) < 128) {
					if (buf[ja] != K(KT_META, KVAL(buf[j])))
						fprintf(stderr, _("impossible: not meta?\n"));
					buf[ja] = K_HOLE;
					zapped[ja] = 1;
				}
			}
		}

		fprintf(fd, "keycode %3d =", i);

		if (isasexpected) {
			/* print only a single entry */
			/* suppress the + for ordinary a-zA-Z */
			print_keysym(kmap, fd, K(KT_LATIN, val), numeric);
			fprintf(fd, "\n");
		} else {
			/* choose between single entry line followed by exceptions,
			   and long line followed by exceptions; avoid VoidSymbol */
			int bad = 0;
			int count = 0;

			for (j = 1; j < keymapnr; j++) {
				if (zapped[j])
					continue;

				if (buf[j] != buf[0])
					bad++;

				if (buf[j] != K_HOLE)
					count++;
			}

			if (bad <= count && bad < keymapnr-1) {
				if (buf[0] != K_HOLE) {
					print_keysym(kmap, fd, buf[0], numeric);
				}
				fprintf(fd, "\n");

				for (j = 1; j < keymapnr; j++) {
					if (buf[j] != buf[0] && !zapped[j]) {
						print_bind(kmap, fd, buf[j], i, kmap->defining[j]-1, numeric);
					}
				}
			} else {
				for (j = 0;
				     j < keymapnr && buf[j] != K_HOLE &&
					(j == 0 || table_shape != UNTIL_HOLE ||
					kmap->defining[j]-1 == kmap->defining[j-1]-1+1);
				     j++) {
					//print_bind(kmap, fd, buf[j], i, kmap->defining[j]-1, numeric);
					print_keysym(kmap, fd, buf[j], numeric);
				}

				fprintf(fd, "\n");

				for (; j < keymapnr; j++) {
					if (buf[j] != K_HOLE) {
						print_bind(kmap, fd, buf[j], i, kmap->defining[j]-1, numeric);
					}
				}
			}
		}
	}
}

void
lk_dump_keymap(struct keymap *kmap, FILE *fd, char table_shape, char numeric)
{
	lk_dump_keymaps(kmap, fd);
	lk_dump_keys(kmap, fd, table_shape, numeric);
	lk_dump_funcs(kmap, fd);
	lk_dump_diacs(kmap, fd);
}
