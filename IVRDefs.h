/*
 * IVRDefs.h - Bouton IVR + menu popup extensible
 *
 * Pour ajouter un nouveau bouton IVR plus tard :
 *   1. Ajouter #define IVR_CMD_NOUVEAU 40003
 *   2. Ajouter une ligne dans OnBnClickedIvrMenu()
 *   3. Ajouter StartIVRNouveau() dans mainDlg.cpp
 */
#pragma once

// Bouton unique "IVR +" dans la barre du bas
#ifndef IDC_IVR_MENU
#define IDC_IVR_MENU    1205
#endif

// Commandes du menu popup (range 40000+ = safe pour user commands MFC)
#define IVR_CMD_ECOLE   40001
#define IVR_CMD_CLASSE  40002
// Futurs : #define IVR_CMD_AUTRE   40003
