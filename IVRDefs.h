/*
 * IVRDefs.h - Bouton IVR + menu popup extensible
 */
#pragma once

// Bouton unique "IVR +" dans la barre du bas
#ifndef IDC_IVR_MENU
#define IDC_IVR_MENU      1205
#endif

// === Lancer une sequence IVR (Francais) ===
#define IVR_CMD_ECOLE     40001
#define IVR_CMD_CLASSE    40002
// === Lancer une sequence IVR (English) ===
#define IVR_CMD_SCHOOL_EN 40003
#define IVR_CMD_CLASS_EN  40004

// === Controles de l'IVR en cours (visibles seulement si IVR actif) ===
#define IVR_CMD_REPLAY    40010  // Rejouer l'etape courante
#define IVR_CMD_SKIP      40011  // Passer a l'etape suivante
#define IVR_CMD_STOP_IVR  40012  // Arreter l'IVR proprement
