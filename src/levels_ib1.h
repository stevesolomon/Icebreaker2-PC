/*
 * src/levels_ib1.h
 *
 * Catalog of the original Icebreaker 1 ("classic") levels exposed
 * through a separate level-select grid.  See levels_ib1.cpp for the
 * actual table.
 */
#ifndef LEVELS_IB1_H
#define LEVELS_IB1_H

struct IB1LevelEntry {
    const char *display_name;   /* underscores → spaces, no quotes */
    const char *filename;       /* basename only, no path or extension */
};

extern const IB1LevelEntry kIB1Catalog[];
extern const int            kIB1LevelCount;

#endif /* LEVELS_IB1_H */
