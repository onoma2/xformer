#include "match_token.h"
#
#include <ctype.h>   // isdigit
#include <stdlib.h>  // rand, strtol
#include <stdio.h>

#include "ops/op.h"
#include "ops/op_enum.h"
#include "helpers.h"

%%{
    machine match_token; # declare our ragel machine

    number = (('-')? [0-9]+) | ([X] [0-9A-F]+) | ([B|R] [0-1]+);

    main := |*
        # NUMBERS
        number        => { MATCH_NUMBER() };

        # OPS
        # variables
        "A"           => { MATCH_OP(E_OP_A); };
        "B"           => { MATCH_OP(E_OP_B); };
        "C"           => { MATCH_OP(E_OP_C); };
        "D"           => { MATCH_OP(E_OP_D); };
        "DRUNK"       => { MATCH_OP(E_OP_DRUNK); };
        "DRUNK.MAX"   => { MATCH_OP(E_OP_DRUNK_MAX); };
        "DRUNK.MIN"   => { MATCH_OP(E_OP_DRUNK_MIN); };
        "DRUNK.WRAP"  => { MATCH_OP(E_OP_DRUNK_WRAP); };
        "FLIP"        => { MATCH_OP(E_OP_FLIP); };
        "I"           => { MATCH_OP(E_OP_I); };
        "O"           => { MATCH_OP(E_OP_O); };
        "O.INC"       => { MATCH_OP(E_OP_O_INC); };
        "O.MAX"       => { MATCH_OP(E_OP_O_MAX); };
        "O.MIN"       => { MATCH_OP(E_OP_O_MIN); };
        "O.WRAP"      => { MATCH_OP(E_OP_O_WRAP); };
        "T"           => { MATCH_OP(E_OP_T); };
        "TIME"        => { MATCH_OP(E_OP_TIME); };
        "TIME.ACT"    => { MATCH_OP(E_OP_TIME_ACT); };
        "LAST"        => { MATCH_OP(E_OP_LAST); };
        "X"           => { MATCH_OP(E_OP_X); };
        "Y"           => { MATCH_OP(E_OP_Y); };
        "Z"           => { MATCH_OP(E_OP_Z); };
        "J"           => { MATCH_OP(E_OP_J); };
        "K"           => { MATCH_OP(E_OP_K); };

        # init
        "INIT"            => { MATCH_OP(E_OP_INIT); };
        "INIT.SCENE"      => { MATCH_OP(E_OP_INIT_SCENE); };
        "INIT.SCRIPT"     => { MATCH_OP(E_OP_INIT_SCRIPT); };
        "INIT.SCRIPT.ALL" => { MATCH_OP(E_OP_INIT_SCRIPT_ALL); };
        "INIT.P"          => { MATCH_OP(E_OP_INIT_P); };
        "INIT.P.ALL"      => { MATCH_OP(E_OP_INIT_P_ALL); };
        "INIT.CV"         => { MATCH_OP(E_OP_INIT_CV); };
        "INIT.CV.ALL"     => { MATCH_OP(E_OP_INIT_CV_ALL); };
        "INIT.TR"         => { MATCH_OP(E_OP_INIT_TR); };
        "INIT.TR.ALL"     => { MATCH_OP(E_OP_INIT_TR_ALL); };
        "INIT.DATA"       => { MATCH_OP(E_OP_INIT_DATA); };
        "INIT.TIME"       => { MATCH_OP(E_OP_INIT_TIME); };

        # turtle
        "@"           => { MATCH_OP(E_OP_TURTLE); };
        "@X"          => { MATCH_OP(E_OP_TURTLE_X); };
        "@Y"          => { MATCH_OP(E_OP_TURTLE_Y); };
        "@MOVE"       => { MATCH_OP(E_OP_TURTLE_MOVE); };
        "@F"          => { MATCH_OP(E_OP_TURTLE_F); };
        "@FX1"        => { MATCH_OP(E_OP_TURTLE_FX1); };
        "@FY1"        => { MATCH_OP(E_OP_TURTLE_FY1); };
        "@FX2"        => { MATCH_OP(E_OP_TURTLE_FX2); };
        "@FY2"        => { MATCH_OP(E_OP_TURTLE_FY2); };
        "@SPEED"      => { MATCH_OP(E_OP_TURTLE_SPEED); };
        "@DIR"        => { MATCH_OP(E_OP_TURTLE_DIR); };
        "@STEP"       => { MATCH_OP(E_OP_TURTLE_STEP); };
        "@BUMP"       => { MATCH_OP(E_OP_TURTLE_BUMP); };
        "@WRAP"       => { MATCH_OP(E_OP_TURTLE_WRAP); };
        "@BOUNCE"     => { MATCH_OP(E_OP_TURTLE_BOUNCE); };
        "@SCRIPT"     => { MATCH_OP(E_OP_TURTLE_SCRIPT); };
        "@SHOW"       => { MATCH_OP(E_OP_TURTLE_SHOW); };

        # metronome
        "M"           => { MATCH_OP(E_OP_M); };
        "M!"          => { MATCH_OP(E_OP_M_SYM_EXCLAMATION); };
        "M.ACT"       => { MATCH_OP(E_OP_M_ACT); };
        "M.A"         => { MATCH_OP(E_OP_M_A); };
        "M.ACT.A"     => { MATCH_OP(E_OP_M_ACT_A); };
        "M.RESET"     => { MATCH_OP(E_OP_M_RESET); };
        "M.RESET.A"   => { MATCH_OP(E_OP_M_RESET_A); };

        # patterns
        "P.N"         => { MATCH_OP(E_OP_P_N); };
        "P"           => { MATCH_OP(E_OP_P); };
        "PN"          => { MATCH_OP(E_OP_PN); };
        "P.L"         => { MATCH_OP(E_OP_P_L); };
        "PN.L"        => { MATCH_OP(E_OP_PN_L); };
        "P.WRAP"      => { MATCH_OP(E_OP_P_WRAP); };
        "PN.WRAP"     => { MATCH_OP(E_OP_PN_WRAP); };
        "P.START"     => { MATCH_OP(E_OP_P_START); };
        "PN.START"    => { MATCH_OP(E_OP_PN_START); };
        "P.END"       => { MATCH_OP(E_OP_P_END); };
        "PN.END"      => { MATCH_OP(E_OP_PN_END); };
        "P.I"         => { MATCH_OP(E_OP_P_I); };
        "PN.I"        => { MATCH_OP(E_OP_PN_I); };
        "P.HERE"      => { MATCH_OP(E_OP_P_HERE); };
        "PN.HERE"     => { MATCH_OP(E_OP_PN_HERE); };
        "P.NEXT"      => { MATCH_OP(E_OP_P_NEXT); };
        "PN.NEXT"     => { MATCH_OP(E_OP_PN_NEXT); };
        "P.PREV"      => { MATCH_OP(E_OP_P_PREV); };
        "PN.PREV"     => { MATCH_OP(E_OP_PN_PREV); };
        "P.INS"       => { MATCH_OP(E_OP_P_INS); };
        "PN.INS"      => { MATCH_OP(E_OP_PN_INS); };
        "P.RM"        => { MATCH_OP(E_OP_P_RM); };
        "PN.RM"       => { MATCH_OP(E_OP_PN_RM); };
        "P.PUSH"      => { MATCH_OP(E_OP_P_PUSH); };
        "PN.PUSH"     => { MATCH_OP(E_OP_PN_PUSH); };
        "P.POP"       => { MATCH_OP(E_OP_P_POP); };
        "PN.POP"      => { MATCH_OP(E_OP_PN_POP); };
        "P.MIN"       => { MATCH_OP(E_OP_P_MIN); };
        "PN.MIN"      => { MATCH_OP(E_OP_PN_MIN); };
        "P.MAX"       => { MATCH_OP(E_OP_P_MAX); };
        "PN.MAX"      => { MATCH_OP(E_OP_PN_MAX); };
        "P.SHUF"      => { MATCH_OP(E_OP_P_SHUF); };
        "PN.SHUF"     => { MATCH_OP(E_OP_PN_SHUF); };
        "P.REV"       => { MATCH_OP(E_OP_P_REV); };
        "PN.REV"      => { MATCH_OP(E_OP_PN_REV); };
        "P.ROT"       => { MATCH_OP(E_OP_P_ROT); };
        "PN.ROT"      => { MATCH_OP(E_OP_PN_ROT); };
        "P.RND"       => { MATCH_OP(E_OP_P_RND); };
        "PN.RND"      => { MATCH_OP(E_OP_PN_RND); };
        "P.+"         => { MATCH_OP(E_OP_P_ADD); };
        "PN.+"        => { MATCH_OP(E_OP_PN_ADD); };
        "P.-"         => { MATCH_OP(E_OP_P_SUB); };
        "PN.-"        => { MATCH_OP(E_OP_PN_SUB); };
        "P.+W"        => { MATCH_OP(E_OP_P_ADDW); };
        "PN.+W"       => { MATCH_OP(E_OP_PN_ADDW); };
        "P.-W"        => { MATCH_OP(E_OP_P_SUBW); };
        "PN.-W"       => { MATCH_OP(E_OP_PN_SUBW); };
        "P.PA"        => { MATCH_OP(E_OP_P_PA); };
        "PN.PA"       => { MATCH_OP(E_OP_PN_PA); };
        "P.PS"        => { MATCH_OP(E_OP_P_PS); };
        "PN.PS"       => { MATCH_OP(E_OP_PN_PS); };
        "P.PM"        => { MATCH_OP(E_OP_P_PM); };
        "PN.PM"       => { MATCH_OP(E_OP_PN_PM); };
        "P.PD"        => { MATCH_OP(E_OP_P_PD); };
        "PN.PD"       => { MATCH_OP(E_OP_PN_PD); };
        "P.PMOD"      => { MATCH_OP(E_OP_P_PMOD); };
        "PN.PMOD"     => { MATCH_OP(E_OP_PN_PMOD); };
        "P.SCALE"     => { MATCH_OP(E_OP_P_SCALE); };
        "PN.SCALE"    => { MATCH_OP(E_OP_PN_SCALE); };
        "P.SUM"       => { MATCH_OP(E_OP_P_SUM); };
        "PN.SUM"      => { MATCH_OP(E_OP_PN_SUM); };
        "P.AVG"       => { MATCH_OP(E_OP_P_AVG); };
        "PN.AVG"      => { MATCH_OP(E_OP_PN_AVG); };
        "P.MINV"      => { MATCH_OP(E_OP_P_MINV); };
        "PN.MINV"     => { MATCH_OP(E_OP_PN_MINV); };
        "P.MAXV"      => { MATCH_OP(E_OP_P_MAXV); };
        "PN.MAXV"     => { MATCH_OP(E_OP_PN_MAXV); };
        "P.FND"       => { MATCH_OP(E_OP_P_FND); };
        "PN.FND"      => { MATCH_OP(E_OP_PN_FND); };
        "RND.P"       => { MATCH_OP(E_OP_RND_P); };
        "RND.PN"      => { MATCH_OP(E_OP_RND_PN); };

        # queue
        "Q"           => { MATCH_OP(E_OP_Q); };
        "Q.AVG"       => { MATCH_OP(E_OP_Q_AVG); };
        "Q.N"         => { MATCH_OP(E_OP_Q_N); };
        "Q.CLR"       => { MATCH_OP(E_OP_Q_CLR); };
        "Q.GRW"       => { MATCH_OP(E_OP_Q_GRW); };
        "Q.SUM"       => { MATCH_OP(E_OP_Q_SUM); };
        "Q.MIN"       => { MATCH_OP(E_OP_Q_MIN); };
        "Q.MAX"       => { MATCH_OP(E_OP_Q_MAX); };
        "Q.RND"       => { MATCH_OP(E_OP_Q_RND); };
        "Q.SRT"       => { MATCH_OP(E_OP_Q_SRT); };
        "Q.REV"       => { MATCH_OP(E_OP_Q_REV); };
        "Q.SH"        => { MATCH_OP(E_OP_Q_SH); };
        "Q.ADD"       => { MATCH_OP(E_OP_Q_ADD); };
        "Q.SUB"       => { MATCH_OP(E_OP_Q_SUB); };
        "Q.MUL"       => { MATCH_OP(E_OP_Q_MUL); };
        "Q.DIV"       => { MATCH_OP(E_OP_Q_DIV); };
        "Q.MOD"       => { MATCH_OP(E_OP_Q_MOD); };
        "Q.I"         => { MATCH_OP(E_OP_Q_I); };
        "Q.2P"        => { MATCH_OP(E_OP_Q_2P); };
        "Q.P2"        => { MATCH_OP(E_OP_Q_P2); };

        # hardware
        "CV"          => { MATCH_OP(E_OP_CV); };
        "CV.OFF"      => { MATCH_OP(E_OP_CV_OFF); };
        "CV.SLEW"     => { MATCH_OP(E_OP_CV_SLEW); };
        "CV.CAL"      => { MATCH_OP(E_OP_CV_CAL); };
        "CV.CAL.RESET" => { MATCH_OP(E_OP_CV_CAL_RESET); };
        "IN"          => { MATCH_OP(E_OP_IN); };
        "IN.SCALE"    => { MATCH_OP(E_OP_IN_SCALE); };
        "IN.CAL.MIN"  => { MATCH_OP(E_OP_IN_CAL_MIN); };
        "IN.CAL.MAX"  => { MATCH_OP(E_OP_IN_CAL_MAX); };
        "IN.CAL.RESET" => { MATCH_OP(E_OP_IN_CAL_RESET); };
        "PARAM"       => { MATCH_OP(E_OP_PARAM); };
        "PARAM.SCALE" => { MATCH_OP(E_OP_PARAM_SCALE); };
        "PARAM.CAL.MIN"  => { MATCH_OP(E_OP_PARAM_CAL_MIN); };
        "PARAM.CAL.MAX"  => { MATCH_OP(E_OP_PARAM_CAL_MAX); };
        "PARAM.CAL.RESET" => { MATCH_OP(E_OP_PARAM_CAL_RESET); };
        "BUS"         => { MATCH_OP(E_OP_BUS); };
        "WBPM.S"      => { MATCH_OP(E_OP_WBPM_S); };
        "WBPM"        => { MATCH_OP(E_OP_WBPM); };
        "BAR"         => { MATCH_OP(E_OP_BAR); };
        "WP.SET"      => { MATCH_OP(E_OP_WP_SET); };
        "WP"          => { MATCH_OP(E_OP_WP); };
        "WR.ACT"      => { MATCH_OP(E_OP_WR_ACT); };
        "WR"          => { MATCH_OP(E_OP_WR); };
        "RT"          => { MATCH_OP(E_OP_RT); };
        "PRM"         => { MATCH_OP(E_OP_PRM); };
        "TR"          => { MATCH_OP(E_OP_TR); };
        "TR.D"        => { MATCH_OP(E_OP_TR_D); };
        "TR.W"        => { MATCH_OP(E_OP_TR_W); };
        "TR.POL"      => { MATCH_OP(E_OP_TR_POL); };
        "TR.TIME"     => { MATCH_OP(E_OP_TR_TIME); };
        "TR.TOG"      => { MATCH_OP(E_OP_TR_TOG); };
        "TR.PULSE"    => { MATCH_OP(E_OP_TR_PULSE); };
        "TR.P"        => { MATCH_OP(E_OP_TR_P); };
        "CV.GET"      => { MATCH_OP(E_OP_CV_GET); };
        "CV.SET"      => { MATCH_OP(E_OP_CV_SET); };
        "MUTE"        => { MATCH_OP(E_OP_MUTE); };
        "STATE"       => { MATCH_OP(E_OP_STATE); };
        "LIVE.OFF"    => { MATCH_OP(E_OP_LIVE_OFF); };
        "LIVE.O"      => { MATCH_OP(E_OP_LIVE_O); };
        "LIVE.DASH"   => { MATCH_OP(E_OP_LIVE_DASH); };
        "LIVE.D"      => { MATCH_OP(E_OP_LIVE_D); };
        "LIVE.GRID"   => { MATCH_OP(E_OP_LIVE_GRID); };
        "LIVE.G"      => { MATCH_OP(E_OP_LIVE_G); };
        "LIVE.VARS"   => { MATCH_OP(E_OP_LIVE_VARS); };
        "LIVE.V"      => { MATCH_OP(E_OP_LIVE_V); };
        "PRINT"       => { MATCH_OP(E_OP_PRINT); };
        "PRT"         => { MATCH_OP(E_OP_PRT); };
        "E.A"         => { MATCH_OP(E_OP_E_A); };
        "E.D"         => { MATCH_OP(E_OP_E_D); };
        "E.T"         => { MATCH_OP(E_OP_E_T); };
        "E.O"         => { MATCH_OP(E_OP_E_O); };
        "E.L"         => { MATCH_OP(E_OP_E_L); };
        "E.R"         => { MATCH_OP(E_OP_E_R); };
        "E.C"         => { MATCH_OP(E_OP_E_C); };
        "E"           => { MATCH_OP(E_OP_E); };

        # maths
        "ADD"         => { MATCH_OP(E_OP_ADD); };
        "SUB"         => { MATCH_OP(E_OP_SUB); };
        "MUL"         => { MATCH_OP(E_OP_MUL); };
        "DIV"         => { MATCH_OP(E_OP_DIV); };
        "MOD"         => { MATCH_OP(E_OP_MOD); };
        "RAND"        => { MATCH_OP(E_OP_RAND); };
        "RND"         => { MATCH_OP(E_OP_RND); };
        "RRAND"       => { MATCH_OP(E_OP_RRAND); };
        "RRND"        => { MATCH_OP(E_OP_RRND); };
        "R"           => { MATCH_OP(E_OP_R); };
        "R.MIN"       => { MATCH_OP(E_OP_R_MIN); };
        "R.MAX"       => { MATCH_OP(E_OP_R_MAX); };
        "TOSS"        => { MATCH_OP(E_OP_TOSS); };
        "MIN"         => { MATCH_OP(E_OP_MIN); };
        "MAX"         => { MATCH_OP(E_OP_MAX); };
        "LIM"         => { MATCH_OP(E_OP_LIM); };
        "WRAP"        => { MATCH_OP(E_OP_WRAP); };
        "WRP"         => { MATCH_OP(E_OP_WRP); };
        "QT"          => { MATCH_OP(E_OP_QT); };
        "QT.S"        => { MATCH_OP(E_OP_QT_S); };
        "QT.CS"       => { MATCH_OP(E_OP_QT_CS); };
        "QT.B"        => { MATCH_OP(E_OP_QT_B); };
        "QT.BX"       => { MATCH_OP(E_OP_QT_BX); };
        "AVG"         => { MATCH_OP(E_OP_AVG); };
        "EQ"          => { MATCH_OP(E_OP_EQ); };
        "NE"          => { MATCH_OP(E_OP_NE); };
        "LT"          => { MATCH_OP(E_OP_LT); };
        "GT"          => { MATCH_OP(E_OP_GT); };
        "LTE"         => { MATCH_OP(E_OP_LTE); };
        "GTE"         => { MATCH_OP(E_OP_GTE); };
        "INR"         => { MATCH_OP(E_OP_INR); };
        "OUTR"        => { MATCH_OP(E_OP_OUTR); };
        "INRI"        => { MATCH_OP(E_OP_INRI); };
        "OUTRI"       => { MATCH_OP(E_OP_OUTRI); };
	    "NZ"          => { MATCH_OP(E_OP_NZ); };
        "EZ"          => { MATCH_OP(E_OP_EZ); };
        "RSH"         => { MATCH_OP(E_OP_RSH); };
        "LSH"         => { MATCH_OP(E_OP_LSH); };
        "RROT"        => { MATCH_OP(E_OP_RROT); };
        "LROT"        => { MATCH_OP(E_OP_LROT); };
        "EXP"         => { MATCH_OP(E_OP_EXP); };
        "ABS"         => { MATCH_OP(E_OP_ABS); };
        "SGN"         => { MATCH_OP(E_OP_SGN); };
        "AND"         => { MATCH_OP(E_OP_AND); };
        "OR"          => { MATCH_OP(E_OP_OR); };
        "AND3"        => { MATCH_OP(E_OP_AND3); };
        "OR3"         => { MATCH_OP(E_OP_OR3); };
        "AND4"        => { MATCH_OP(E_OP_AND4); };
        "OR4"         => { MATCH_OP(E_OP_OR4); };
        "JI"          => { MATCH_OP(E_OP_JI); };
        "SCALE"       => { MATCH_OP(E_OP_SCALE); };
        "SCL"         => { MATCH_OP(E_OP_SCL); };
        "SCALE0"      => { MATCH_OP(E_OP_SCALE0); };
        "SCL0"        => { MATCH_OP(E_OP_SCL0); };
        "N"           => { MATCH_OP(E_OP_N); };
        "VN"          => { MATCH_OP(E_OP_VN); };
        "HZ"          => { MATCH_OP(E_OP_HZ); };
        "N.S"         => { MATCH_OP(E_OP_N_S); };
        "N.C"         => { MATCH_OP(E_OP_N_C); };
        "N.CS"        => { MATCH_OP(E_OP_N_CS); };
        "N.B"         => { MATCH_OP(E_OP_N_B); };
        "N.BX"        => { MATCH_OP(E_OP_N_BX); };
        "V"           => { MATCH_OP(E_OP_V); };
        "VV"          => { MATCH_OP(E_OP_VV); };
        "ER"          => { MATCH_OP(E_OP_ER); };
        "NR"          => { MATCH_OP(E_OP_NR); };
        "DR.T"        => { MATCH_OP(E_OP_DR_T); };
        "DR.P"        => { MATCH_OP(E_OP_DR_P); };
        "DR.V"        => { MATCH_OP(E_OP_DR_V); };
        "BPM"         => { MATCH_OP(E_OP_BPM);; };
        "|"           => { MATCH_OP(E_OP_BIT_OR);; };
        "&"           => { MATCH_OP(E_OP_BIT_AND);; };
        "~"           => { MATCH_OP(E_OP_BIT_NOT);; };
        "^"           => { MATCH_OP(E_OP_BIT_XOR);; };
        "BSET"        => { MATCH_OP(E_OP_BSET);; };
        "BGET"        => { MATCH_OP(E_OP_BGET);; };
        "BCLR"        => { MATCH_OP(E_OP_BCLR);; };
        "BTOG"        => { MATCH_OP(E_OP_BTOG);; };
        "BREV"        => { MATCH_OP(E_OP_BREV);; };
        "XOR"         => { MATCH_OP(E_OP_XOR); };
        "CHAOS"       => { MATCH_OP(E_OP_CHAOS); };
        "CHAOS.R"     => { MATCH_OP(E_OP_CHAOS_R); };
        "CHAOS.ALG"   => { MATCH_OP(E_OP_CHAOS_ALG); };
        "?"           => { MATCH_OP(E_OP_TIF); };
        "+"           => { MATCH_OP(E_OP_SYM_PLUS); };
        "-"           => { MATCH_OP(E_OP_SYM_DASH); };
        "*"           => { MATCH_OP(E_OP_SYM_STAR); };
        "/"           => { MATCH_OP(E_OP_SYM_FORWARD_SLASH); };
        "%"           => { MATCH_OP(E_OP_SYM_PERCENTAGE); };
        "=="          => { MATCH_OP(E_OP_SYM_EQUAL_x2); };
        "!="          => { MATCH_OP(E_OP_SYM_EXCLAMATION_EQUAL); };
        "<"           => { MATCH_OP(E_OP_SYM_LEFT_ANGLED); };
        ">"           => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED); };
        "<="          => { MATCH_OP(E_OP_SYM_LEFT_ANGLED_EQUAL); };
        ">="          => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED_EQUAL); };
        "><"          => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED_LEFT_ANGLED); };
        "<>"          => { MATCH_OP(E_OP_SYM_LEFT_ANGLED_RIGHT_ANGLED); };
        ">=<"         => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED_EQUAL_LEFT_ANGLED); };
        "<=>"         => { MATCH_OP(E_OP_SYM_LEFT_ANGLED_EQUAL_RIGHT_ANGLED); };
	    "!"           => { MATCH_OP(E_OP_SYM_EXCLAMATION); };
        "<<"          => { MATCH_OP(E_OP_SYM_LEFT_ANGLED_x2); };
        ">>"          => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED_x2); };
        "<<<"         => { MATCH_OP(E_OP_SYM_LEFT_ANGLED_x3); };
        ">>>"         => { MATCH_OP(E_OP_SYM_RIGHT_ANGLED_x3); };
        "&&"          => { MATCH_OP(E_OP_SYM_AMPERSAND_x2); };
        "||"          => { MATCH_OP(E_OP_SYM_PIPE_x2); };
        "&&&"         => { MATCH_OP(E_OP_SYM_AMPERSAND_x3); };
        "|||"         => { MATCH_OP(E_OP_SYM_PIPE_x3); };
        "&&&&"        => { MATCH_OP(E_OP_SYM_AMPERSAND_x4); };
        "||||"        => { MATCH_OP(E_OP_SYM_PIPE_x4); };

        # stack
        "S.ALL"       => { MATCH_OP(E_OP_S_ALL); };
        "S.POP"       => { MATCH_OP(E_OP_S_POP); };
        "S.CLR"       => { MATCH_OP(E_OP_S_CLR); };
        "S.L"         => { MATCH_OP(E_OP_S_L); };

        # controlflow
        "SCRIPT"      => { MATCH_OP(E_OP_SCRIPT); };
        "$"           => { MATCH_OP(E_OP_SYM_DOLLAR); };
        "SCRIPT.POL"  => { MATCH_OP(E_OP_SCRIPT_POL); };
        "$.POL"       => { MATCH_OP(E_OP_SYM_DOLLAR_POL); };
        "KILL"        => { MATCH_OP(E_OP_KILL); };
        "SCENE"       => { MATCH_OP(E_OP_SCENE); };
        "SCENE.G"     => { MATCH_OP(E_OP_SCENE_G); };
        "SCENE.P"     => { MATCH_OP(E_OP_SCENE_P); };
        "BREAK"       => { MATCH_OP(E_OP_BREAK); };
        "BRK"         => { MATCH_OP(E_OP_BRK); };
        "SYNC"        => { MATCH_OP(E_OP_SYNC); };
        "$F"          => { MATCH_OP(E_OP_SYM_DOLLAR_F); };
        "$F1"         => { MATCH_OP(E_OP_SYM_DOLLAR_F1); };
        "$F2"         => { MATCH_OP(E_OP_SYM_DOLLAR_F2); };
        "$L"          => { MATCH_OP(E_OP_SYM_DOLLAR_L); };
        "$L1"         => { MATCH_OP(E_OP_SYM_DOLLAR_L1); };
        "$L2"         => { MATCH_OP(E_OP_SYM_DOLLAR_L2); };
        "$S"          => { MATCH_OP(E_OP_SYM_DOLLAR_S); };
        "$S1"         => { MATCH_OP(E_OP_SYM_DOLLAR_S1); };
        "$S2"         => { MATCH_OP(E_OP_SYM_DOLLAR_S2); };
        "I1"          => { MATCH_OP(E_OP_I1); };
        "I2"          => { MATCH_OP(E_OP_I2); };
        "FR"          => { MATCH_OP(E_OP_FR); };

        # delay
        "DEL.CLR"     => { MATCH_OP(E_OP_DEL_CLR); };

        # seed
        "SEED"        => { MATCH_OP(E_OP_SEED); };
        "RAND.SEED"      => { MATCH_OP(E_OP_RAND_SEED); };
        "RAND.SD"      => { MATCH_OP(E_OP_SYM_RAND_SD); };
        "R.SD"          => { MATCH_OP(E_OP_SYM_R_SD); };
        "TOSS.SEED"   => { MATCH_OP(E_OP_TOSS_SEED); };
        "TOSS.SD"      => { MATCH_OP(E_OP_SYM_TOSS_SD); };
        "PROB.SEED"   => { MATCH_OP(E_OP_PROB_SEED); };
        "PROB.SD"      => { MATCH_OP(E_OP_SYM_PROB_SD); };
        "DRUNK.SEED"  => { MATCH_OP(E_OP_DRUNK_SEED); };
        "DRUNK.SD"      => { MATCH_OP(E_OP_SYM_DRUNK_SD); };
        "P.SEED"      => { MATCH_OP(E_OP_P_SEED); };
        "P.SD"          => { MATCH_OP(E_OP_SYM_P_SD); };

        # MIDI
        "MI.$"        => { MATCH_OP(E_OP_MI_SYM_DOLLAR); };
        "MI.LE"       => { MATCH_OP(E_OP_MI_LE); };
        "MI.LN"       => { MATCH_OP(E_OP_MI_LN); };
        "MI.LNV"      => { MATCH_OP(E_OP_MI_LNV); };
        "MI.LV"       => { MATCH_OP(E_OP_MI_LV); };
        "MI.LVV"      => { MATCH_OP(E_OP_MI_LVV); };
        "MI.LO"       => { MATCH_OP(E_OP_MI_LO); };
        "MI.LC"       => { MATCH_OP(E_OP_MI_LC); };
        "MI.LCC"      => { MATCH_OP(E_OP_MI_LCC); };
        "MI.LCCV"     => { MATCH_OP(E_OP_MI_LCCV); };
        "MI.NL"       => { MATCH_OP(E_OP_MI_NL); };
        "MI.N"        => { MATCH_OP(E_OP_MI_N); };
        "MI.NV"       => { MATCH_OP(E_OP_MI_NV); };
        "MI.V"        => { MATCH_OP(E_OP_MI_V); };
        "MI.VV"       => { MATCH_OP(E_OP_MI_VV); };
        "MI.OL"       => { MATCH_OP(E_OP_MI_OL); };
        "MI.O"        => { MATCH_OP(E_OP_MI_O); };
        "MI.CL"       => { MATCH_OP(E_OP_MI_CL); };
        "MI.C"        => { MATCH_OP(E_OP_MI_C); };
        "MI.CC"       => { MATCH_OP(E_OP_MI_CC); };
        "MI.CCV"      => { MATCH_OP(E_OP_MI_CCV); };
        "MI.LCH"      => { MATCH_OP(E_OP_MI_LCH); };
        "MI.NCH"      => { MATCH_OP(E_OP_MI_NCH); };
        "MI.OCH"      => { MATCH_OP(E_OP_MI_OCH); };
        "MI.CCH"      => { MATCH_OP(E_OP_MI_CCH); };
        "MI.CLKD"     => { MATCH_OP(E_OP_MI_CLKD); };
        "MI.CLKR"     => { MATCH_OP(E_OP_MI_CLKR); };

        # MODS
        # controlflow
        "IF"          => { MATCH_MOD(E_MOD_IF); };
        "ELIF"        => { MATCH_MOD(E_MOD_ELIF); };
        "ELSE"        => { MATCH_MOD(E_MOD_ELSE); };
        "L"           => { MATCH_MOD(E_MOD_L); };
        "W"           => { MATCH_MOD(E_MOD_W); };
        "EVERY"       => { MATCH_MOD(E_MOD_EVERY); };
        "EV"          => { MATCH_MOD(E_MOD_EV); };
        "SKIP"        => { MATCH_MOD(E_MOD_SKIP); };
        "OTHER"       => { MATCH_MOD(E_MOD_OTHER); };
        # delay
        "PROB"        => { MATCH_MOD(E_MOD_PROB); };
        "DEL"         => { MATCH_MOD(E_MOD_DEL); };
        "DEL.X"       => { MATCH_MOD(E_MOD_DEL_X); };
        "DEL.R"       => { MATCH_MOD(E_MOD_DEL_R); };
        "DEL.G"       => { MATCH_MOD(E_MOD_DEL_G); };
        "DEL.B"       => { MATCH_MOD(E_MOD_DEL_B); };

        # pattern
        "P.MAP"       => { MATCH_MOD(E_MOD_P_MAP); };
        "PN.MAP"      => { MATCH_MOD(E_MOD_PN_MAP); };

        # stack
        "S"           => { MATCH_MOD(E_MOD_S); };
    *|;

    write data;          # write any ragel data here
}%%

// these are our macros that are inserted into the code when Ragel finds a match
#define MATCH_OP(op) { out->tag = OP; out->value = op; no_of_tokens++; }
#define MATCH_MOD(mod) { out->tag = MOD; out->value = mod; no_of_tokens++; }
#define MATCH_NUMBER()                                   \
    {                                                    \
        out->tag = NUMBER;                               \
        uint8_t base = 0;                                \
        uint8_t binhex = 0;                              \
        uint8_t bitrev = 0;                              \
        if (token[0] == 'X') {                           \
            out->tag = XNUMBER;                          \
            binhex = 1;                                  \
            base = 16;                                   \
            token++;                                     \
        }                                                \
        else if (token[0] == 'B') {                      \
            out->tag = BNUMBER;                          \
            binhex = 1;                                  \
            base = 2;                                    \
            token++;                                     \
        }                                                \
        else if (token[0] == 'R') {                      \
            out->tag = RNUMBER;                          \
            binhex = 1;                                  \
            bitrev = 1;                                  \
            base = 2;                                    \
            token++;                                     \
        }                                                \
        int32_t val = strtol(token, NULL, base);         \
        if (binhex) val = (int16_t)((uint16_t)val);      \
        if (bitrev) val = rev_bitstring_to_int(token);         \
        val = val > INT16_MAX ? INT16_MAX : val;         \
        val = val < INT16_MIN ? INT16_MIN : val;         \
        out->value = val;                                \
        no_of_tokens++;                                  \
    }

// matches a single token, out contains the token, return value indicates
// success or failure
bool match_token(const char *token, const size_t len, tele_data_t *out) {
    // required ragel declarations
    int cs;                       // machine state
    int act;                      // used with '=>'
    const char* ts;               // token start
    const char* te;               // token end

    const char* p = token;        // pointer to data
    const char* pe = token + len; // pointer to end of data
    const char* eof = pe;         // pointer to eof
    (void)match_token_en_main;    // fix unused variable warning
    (void)ts;                     // fix unused variable warning

    int no_of_tokens = 0;

    %%{
        write init; # write initialisation
        write exec; # run the machine
    }%%

    // Ragel errors
    if (cs == match_token_error || cs < match_token_first_final) {
        return false;
    }
    // only 1 token!
    else if (no_of_tokens != 1) {
        return false;
    }
    else {
        return true;
    }

}
