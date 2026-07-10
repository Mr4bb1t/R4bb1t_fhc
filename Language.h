#pragma once
#include <Arduino.h>

struct LangPack {
    // ── Menu Principal ──
    const char* main_lbl_wifi;
    const char* main_lbl_24ghz;
    const char* main_lbl_subghz;
    const char* main_lbl_config;
    const char* main_hdr_home;
    const char* main_hdr_wifi;
    const char* main_scanning;

    // ── Menu Config ──
    const char* cfg_itm_voltar;
    const char* cfg_itm_sobre;
    const char* cfg_itm_mac;
    const char* cfg_itm_brilho;
    const char* cfg_itm_modomenu;
    const char* cfg_itm_storage;
    const char* cfg_itm_saver;
    const char* cfg_itm_idioma;
    const char* cfg_itm_desligar;
    const char* cfg_hdr_settings;
    const char* cfg_hdr_modomenu;
    const char* cfg_mm_bloco;
    const char* cfg_mm_lista;
    const char* cfg_hdr_idioma;
    const char* cfg_lang_pt;
    const char* cfg_lang_en;
    const char* cfg_lang_hint;
    const char* cfg_lang_salvo;
    const char* cfg_hdr_mac;
    const char* cfg_mac_atual;
    const char* cfg_mac_gerar;
    const char* cfg_mac_sel_gerar;
    const char* cfg_mac_novo;
    const char* cfg_mac_sel_aplicar;
    const char* cfg_mac_aplicado;
    const char* cfg_mac_sel_voltar;

    // ── Sobre ──
    const char* cfg_lbl_chip;
    const char* cfg_lbl_cores;
    const char* cfg_lbl_rev;
    const char* cfg_lbl_flash;
    const char* cfg_lbl_heap;
    const char* cfg_lbl_sdk;
    const char* cfg_lbl_fw;
    const char* cfg_lbl_mac;
    const char* cfg_st_conectado;
    const char* cfg_st_nao_detect;
    const char* cfg_hdr_nrf;
    const char* cfg_lbl_bus;
    const char* cfg_lbl_ce;
    const char* cfg_lbl_csn;
    const char* cfg_lbl_sck;
    const char* cfg_lbl_miso;
    const char* cfg_lbl_mosi;
    const char* cfg_val_hspi;
    const char* cfg_hdr_nrf2;
    const char* cfg_hdr_cc1101;
    const char* cfg_lbl_freq;
    const char* cfg_lbl_cs;
    const char* cfg_lbl_gdo0;
    const char* cfg_lbl_gdo2;
    const char* cfg_hdr_bateria;
    const char* cfg_lbl_tensao;
    const char* cfg_lbl_adcpin;
    const char* cfg_sym_pct;

    // ── Brilho ──
    const char* cfg_hdr_brilho;
    const char* cfg_bl_min;
    const char* cfg_bl_max;
    const char* cfg_bl_hint1;
    const char* cfg_bl_hint2;

    // ── Armazenamento ──
    const char* cfg_hdr_storage;
    const char* cfg_st_used;
    const char* cfg_st_voltar;
    const char* cfg_st_arquivos;
    const char* cfg_st_hint;
    const char* cfg_st_back;
    const char* cfg_st_hint2;
    const char* cfg_st_galeria;
    const char* cfg_st_sair;
    const char* cfg_st_hint3;

    // ── Desligar ──
    const char* cfg_hdr_desligar;
    const char* cfg_dl_msg;
    const char* cfg_dl_cancelar;
    const char* cfg_dl_confirmar;
    const char* cfg_dl_sistema;
    const char* cfg_dl_aguarde;

    // ── Descanso Tela ──
    const char* cfg_hdr_saver;
    const char* cfg_sv_voltar;
    const char* cfg_sv_ativo;
    const char* cfg_sv_hint1;
    const char* cfg_sv_hint2;

    // ── Bateria Niveis ──
    const char* cfg_bat_critico;
    const char* cfg_bat_baixo;
    const char* cfg_bat_medio;
    const char* cfg_bat_bom;
    const char* cfg_bat_cheio;

    // ── Menu RF ──
    const char* rf_itm_voltar;
    const char* rf_itm_capturar;
    const char* rf_itm_sniffer;
    const char* rf_itm_grafico;
    const char* rf_itm_jammer;
    const char* rf_itm_salvos;
    const char* rf_hdr_subghz;

    // ── RF Replay ──
    const char* rf_hdr_replay;
    const char* rf_rpl_aguardando;
    const char* rf_rpl_hint1;
    const char* rf_rpl_hint2;
    const char* rf_rpl_capturado;
    const char* rf_rpl_hint_tx;
    const char* rf_hint_voltar;
    const char* rf_rpl_enviado;
    const char* rf_rpl_salvo;
    const char* rf_rpl_erro_salvar;

    // ── RF Raw ──
    const char* rf_hdr_raw;
    const char* rf_raw_escutando;

    // ── RF Analyser ──
    const char* rf_hdr_analyser;
    const char* rf_anl_scan;

    // ── RF Jammer ──
    const char* rf_hdr_jammer;
    const char* rf_jam_indisponivel;
    const char* rf_jam_desc1;
    const char* rf_jam_desc2;
    const char* rf_jam_desc3;
    const char* rf_jam_hint_start;
    const char* rf_jam_ativo;
    const char* rf_jam_hint_stop;

    // ── RF Saved ──
    const char* rf_hdr_saved;
    const char* rf_svd_nenhum;
    const char* rf_svd_hint1;
    const char* rf_svd_hint2;
    const char* rf_svd_back;
    const char* rf_svd_acoes;
    const char* rf_svd_transmitir;
    const char* rf_svd_excluir;
    const char* rf_svd_repetir;
    const char* rf_svd_voltar;
    const char* rf_svd_enviado;
    const char* rf_svd_deletado;
    const char* rf_svd_parar;
    const char* rf_svd_pulsos;

    // ── Menu Ataques ──
    const char* atk_hdr_wifi;
    const char* atk_itm_back;
    const char* atk_itm_captive;
    const char* atk_itm_deauther;
    const char* atk_itm_analyzer;
    const char* atk_itm_beaconspam;

    // ── Captive Portal ──
    const char* atk_hdr_captive;
    const char* atk_cp_ativo;
    const char* atk_cp_portal;
    const char* atk_cp_deauth;
    const char* atk_cp_apagar;
    const char* atk_cp_credenciais;

    // ── Apagar Dados ──
    const char* atk_hdr_apagar;
    const char* atk_ap_msg;
    const char* atk_ap_irreversivel;
    const char* atk_ap_cancelar;
    const char* atk_ap_confirmar;
    const char* atk_ap_cred_apagadas;

    // ── Deauther ──
    const char* atk_hdr_deauther;
    const char* atk_da_back;
    const char* atk_da_broadcast;
    const char* atk_da_targeted;
    const char* atk_da_iniciar;
    const char* atk_da_hint;
    const char* atk_da_bcast;
    const char* atk_da_trgd;
    const char* atk_da_parar;
    const char* atk_da_parado;
    const char* atk_da_enviados;
    const char* atk_da_erro_radio;

    // ── Deauther Scan ──
    const char* atk_hdr_clientes;
    const char* atk_ds_parar_scan;
    const char* atk_ds_escanear;
    const char* atk_ds_voltar;
    const char* atk_ds_scan;
    const char* atk_ds_nenhum;
    const char* atk_ds_encontrado;
    const char* atk_ds_mac_fmt;
    const char* atk_ds_clientes_fmt;
    const char* atk_ds_hint;

    // ── WiFi Analyzer ──
    const char* atk_hdr_analyzer;
    const char* atk_anl_packets_sec;
    const char* atk_anl_total_packets;
    const char* atk_anl_clients;
    const char* atk_anl_compare_vs;
    const char* atk_anl_channel;
    const char* atk_anl_auth;

    // ── Beacon Modo ──
    const char* atk_hdr_beaconmodo;
    const char* atk_bm_back;
    const char* atk_bm_copia;
    const char* atk_bm_aleatorio;
    const char* atk_bm_personalizado;

    // ── Beacon Custom ──
    const char* atk_hdr_nomecustom;

    // ── Beacon Spam ──
    const char* atk_hdr_beaconspam;
    const char* atk_bs_aleatorio;
    const char* atk_bs_pool;
    const char* atk_bs_hint_pool;
    const char* atk_bs_hint_iniciar;
    const char* atk_bs_hint_voltar;
    const char* atk_bs_ativo;
    const char* atk_bs_beacons;
    const char* atk_bs_parar;

    // ── Credenciais ──
    const char* atk_hdr_credenciais;
    const char* atk_cr_nenhuma;

    // ── Monitor de Pacotes ──
    const char* pm_hdr_monitor;
    const char* pm_lbl_sair;
    const char* pm_lbl_grafico;
    const char* pm_lbl_pkts_s;
    const char* pm_lbl_total;
    const char* pm_lbl_devices;
    const char* pm_lbl_deauth_s;
    const char* pm_lbl_rx;
    const char* pm_lbl_tx;
    const char* pm_lbl_max;
    const char* pm_lbl_avg;

    // ── Menu NRF24 ──
    const char* nrf_itm_back;
    const char* nrf_itm_btjammer;
    const char* nrf_desc_bt;
    const char* nrf_itm_dronejammer;
    const char* nrf_desc_drone;
    const char* nrf_itm_bleadvjammer;
    const char* nrf_desc_bleadv;
    const char* nrf_itm_bledatajammer;
    const char* nrf_desc_bledata;
    const char* nrf_itm_zigbeejammer;
    const char* nrf_desc_zigbee;
    const char* nrf_itm_miscjammer;
    const char* nrf_desc_misc;
    const char* nrf_hdr_jammer;
    const char* nrf_hdr_attack;
    const char* nrf_lbl_modulos;
    const char* nrf_st_init;
    const char* nrf_st_ativo;
    const char* nrf_st_inativo;
    const char* nrf_lbl_canal;
    const char* nrf_lbl_pacotes;
    const char* nrf_hint_startstop;
    const char* nrf_hint_back;
    const char* nrf_lbl_mod1;
    const char* nrf_lbl_mod2;

    // ── Menu Networks ──
    const char* net_hdr_wifi;
    const char* net_itm_back;

    // ── Main (low battery / errors) ──
    const char* sys_bat_critica;
    const char* sys_bat_conecte;
    const char* sys_bat_pct;
    const char* sys_bat_desligando;
    const char* sys_err_mutex;
    const char* sys_err_spiffs;

    // ── Splash ──
    const char* spl_bmp_nao;
    const char* spl_bmp_formato;
    const char* spl_bmp_uncompressed;
    const char* spl_memoria;

    // ── Misc ──
    const char* atk_lbl_pkt;
    const char* atk_kbd_shift;
    const char* atk_kbd_del;
    const char* atk_kbd_enter;
    const char* atk_bs_pool_lbl;

    // ── Hard Reset & First Boot ──
    const char* cfg_itm_hardreset;
    const char* hr_hdr_reset;
    const char* hr_conf1_msg1;
    const char* hr_conf1_msg2;
    const char* hr_conf1_msg3;
    const char* hr_conf2_msg1;
    const char* hr_conf2_msg2;
    const char* hr_btn_cancelar;
    const char* hr_btn_proximo;
    const char* hr_btn_confirmar;
    const char* hr_msg_apagando;
    const char* fb_sel_idioma;
    const char* fb_bemvindo;

    // ── Storage labels ──
    const char* st_fw;
    const char* st_spiffs;
    const char* st_free;
    const char* st_total;

    // ── Screensaver anim names ──
    const char* sv_name_logo;
    const char* sv_name_matrix;
    const char* sv_name_cubo;
    const char* sv_name_plasma;
    const char* sv_name_tesseract;
    const char* sv_name_corredor;
    const char* sv_name_ondagrade;
    const char* sv_name_aneis;
    const char* sv_name_quadrados;
    const char* sv_name_olho;
    const char* sv_name_desativar;
};

extern const LangPack* lang;
void setLanguage(int langId);
