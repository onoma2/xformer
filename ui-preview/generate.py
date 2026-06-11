#!/usr/bin/env python3
"""
UI Preview Generator

Generates 256x64 OLED preview images matching the firmware edit pages.
Run directly:
    # Firmware edit pages
    python3 tools/ui-preview/generate.py --page note-edit -o note.png
    python3 tools/ui-preview/generate.py --page discrete-map-edit -o dm.png
    python3 tools/ui-preview/generate.py --page tuesday-edit -o tue.png
    python3 tools/ui-preview/generate.py --page stochastic-edit -o sto.png

    # Stochastic track pages (from UI-DESIGN.md)
    python3 tools/ui-preview/generate.py --page stochastic-steps -o steps.png
    python3 tools/ui-preview/generate.py --page stochastic-dice -o dice.png
    python3 tools/ui-preview/generate.py --page stochastic-pitch -o pitch.png
    python3 tools/ui-preview/generate.py --page stochastic-marbles -o marbles.png
    python3 tools/ui-preview/generate.py --page stochastic-lock -o lock.png
    python3 tools/ui-preview/generate.py --page stochastic-track -o track.png

    # External reference screens
    python3 tools/ui-preview/generate.py --page ref-prob-melod -o probmelod.png
    python3 tools/ui-preview/generate.py --page ref-shredder -o shredder.png
    python3 tools/ui-preview/generate.py --page ref-euclid -o euclid.png
    python3 tools/ui-preview/generate.py --page ref-delinquencer -o delinq.png
    python3 tools/ui-preview/generate.py --page ref-skylines -o skylines.png
    python3 tools/ui-preview/generate.py --page ref-prob-div -o probdiv.png

The image is upscaled 4x by default so individual pixels are visible.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import FrameBuffer, Canvas, Font, framebuffer_to_image
from pages import (
    render_note_sequence_edit_page,
    render_discrete_map_sequence_page,
    render_tuesday_edit_page,
    render_stochastic_edit_page,
    render_stochastic_steps,
    render_stochastic_dice,
    render_stochastic_pitch,
    render_stochastic_marbles,
    render_stochastic_lock,
    render_stochastic_track,
    # Hybrid reference-crossover pages
    render_stochastic_steps_skylines,
    render_stochastic_steps_delinquencer,
    render_stochastic_steps_euclid,
    render_stochastic_pitch_prob_melod,
    render_stochastic_pitch_circle,
    render_stochastic_pitch_less_concepts,
    render_stochastic_dice_shredder,
    render_stochastic_dice_register,
    render_stochastic_marbles_rndwalk,
    render_stochastic_marbles_constellations,
    render_stochastic_lock_skylines,
    render_stochastic_lock_qfwfq,
    render_stochastic_track_rndwalk,
    render_stochastic_track_sequencex,
    # Norns-inspired hybrid pages (second wave)
    render_stochastic_steps_bline,
    render_stochastic_steps_hiswing,
    render_stochastic_steps_meadowphysics,
    render_stochastic_pitch_kreislauf,
    render_stochastic_pitch_bline,
    render_stochastic_dice_grd,
    render_stochastic_dice_kreislauf,
    render_stochastic_marbles_pit_orchisstra,
    render_stochastic_marbles_bline,
    render_stochastic_lock_meadowphysics,
    render_stochastic_lock_pit_orchisstra,
    render_stochastic_track_bline,
    render_stochastic_track_pitter_patter,
    # Pure reference screens
    render_ref_prob_melod,
    render_ref_shredder,
    render_ref_euclid,
    render_ref_delinquencer,
    render_ref_skylines,
    render_ref_prob_div,
)
from pages_stochastic_loop import (
    render_stochastic_loop_proposed,
)
from pages_stochastic_live import (
    render_stochastic_live_proposed,
    render_stochastic_live_constellation,
)
from pages_modulator import (
    render_modulator_current_sine,
    render_modulator_current_adsr,
    render_modulator_current_random,
    render_modulator_current_chaos,
    render_modulator_proposed_sine,
    render_modulator_proposed_adsr,
    render_modulator_proposed_random,
    render_modulator_proposed_chaos,
    render_modulator_proposed_adsr_page2,
    render_modulator_proposed_chaos_page2,
    render_modulator_rate_free_proposed,
    render_modulator_rate_tempo_proposed,
    render_modulator_gatemodes_proposed,
    render_modulator_scope_proposed,
    render_modulator_destinations_grid_proposed,
    render_modulator_justf_time,
    render_modulator_justf_intone,
    render_modulator_justf_follower,
    render_modulator_justf_adsr_follower,
    render_modulator_justf_adsr_intone,
    render_modulator_geode_globals,
    render_modulator_geode_voice,
    render_modulator_adsr_floor,
    render_modulator_list_sine,
    render_modulator_list_adsr,
    render_modulator_list_random,
    render_modulator_list_chaos,
    render_modulator_list_random_uniform,
    render_modulator_list_spring,
    render_modulator_list_spring_p2,
    render_modulator_list_chaos_p2,
    render_modulator_list_chaos_p1,
    render_modulator_list_adsr_p2,
    render_modulator_list_adsr_p1,
    render_modulator_list_lfo,
)
from tracks import (
    DiscreteMapSequence, DiscreteMapTrackEngine,
    NoteSequence, NoteSequenceStep, NoteTrackEngine,
    TuesdaySequence, TuesdayTrackEngine,
    StochasticSequence, StochasticSequenceStep, StochasticTrackEngine,
)


# ---------------------------------------------------------------------------
# Demo data builders
# ---------------------------------------------------------------------------
def _make_note_data():
    steps = []
    for i in range(64):
        gate = (i % 3) != 0
        steps.append(NoteSequenceStep(
            gate=gate,
            gate_probability=i % 8,
            gate_offset=(i % 5) - 2,
            retrigger=i % 4,
            length=i % 8,
            note=i % 12,
            slide=(i % 7) == 0,
            condition=i % 16,
        ))
    seq = NoteSequence(steps=steps, first_step=0, last_step=15)
    engine = NoteTrackEngine(current_step=3)
    return seq, engine


def _make_discrete_map_data():
    stages = [
        DiscreteMapSequence.Stage(threshold=-80, direction=DiscreteMapSequence.Stage.TriggerDir.Rise, note_index=0),
        DiscreteMapSequence.Stage(threshold=-40, direction=DiscreteMapSequence.Stage.TriggerDir.Rise, note_index=2),
        DiscreteMapSequence.Stage(threshold=0, direction=DiscreteMapSequence.Stage.TriggerDir.Both, note_index=4),
        DiscreteMapSequence.Stage(threshold=40, direction=DiscreteMapSequence.Stage.TriggerDir.Fall, note_index=7),
        DiscreteMapSequence.Stage(threshold=80, direction=DiscreteMapSequence.Stage.TriggerDir.Rise, note_index=9),
    ]
    seq = DiscreteMapSequence(stages=stages, range_low=-5.0, range_high=5.0)
    engine = DiscreteMapTrackEngine(active_stage=2, current_input=1.2)
    return seq, engine


def _make_tuesday_data():
    seq = TuesdaySequence(
        algorithm=5, loop_length=16, flow=12, ornament=14, power=10,
        rotate=3, glide=25, skew=-2, gate_length=60, gate_offset=10,
        step_trill=30, start=0,
    )
    engine = TuesdayTrackEngine(current_step=7, gate_output=True)
    return seq, engine


def _make_stochastic_data():
    steps = []
    for i in range(64):
        gate_prob = 100 if i % 3 == 0 else (50 if i % 2 == 0 else 25)
        note_prob = 100 if i % 4 == 0 else (75 if i % 2 == 0 else 50)
        len_prob = 100 if i % 5 == 0 else 100
        rtrg_prob = 100 if i % 7 == 0 else 0
        steps.append(StochasticSequenceStep(
            gate=(gate_prob > 0),
            gate_probability=gate_prob,
            note=i % 12,
            note_probability=note_prob,
            length=3 + (i % 5),
            length_probability=len_prob,
            retrigger=i % 3,
            retrigger_probability=rtrg_prob,
            condition=i % 16,
            slide=(i % 7) == 0,
        ))
    # Pitch page: varied tickets (some excluded, some zero, some high)
    tickets = [3, -1, 5, 0, 8, 2, -1, 6]
    seq = StochasticSequence(
        steps=steps,
        degree_tickets=tickets,
        scale_size=8,
        scale_root=0,
        degree_rotation=1,
        mask_rotation=0,
        linearity=60,
        min_degree=1,
        max_degree=6,
        marbles_spread=50,
        marbles_bias=-10,
        marbles_steps=5,
        marbles_mode=1,
        lock_enabled=True,
        lock_first=4,
        lock_last=28,
        lock_rotate=2,
        octave=0,
        transpose=0,
        division=4,
        gbias=10,
        nbias=-5,
        lbias=0,
        rbias=0,
        accent=30,
        legato=20,
        fill=50,
        low_octave=-1,
        high_octave=1,
    )
    engine = StochasticTrackEngine(current_step=5, current_note=7, gate_output=True)
    return seq, engine


# ---------------------------------------------------------------------------
# Render dispatch
# ---------------------------------------------------------------------------
def render_page(page: str, scale: int = 4):
    fb = FrameBuffer(256, 64)
    canvas = Canvas(fb, brightness=1.0)
    canvas.set_font(Font.Tiny)

    if page == "note-edit":
        seq, engine = _make_note_data()
        render_note_sequence_edit_page(canvas, seq, engine, layer_index=0, section=0,
                                       step_selection={3, 4}, show_detail=False)
    elif page == "discrete-map-edit":
        seq, engine = _make_discrete_map_data()
        render_discrete_map_sequence_page(canvas, seq, engine, section=0,
                                          edit_mode="threshold", selection_mask=0b101, selected_stage=0)
    elif page == "tuesday-edit":
        seq, engine = _make_tuesday_data()
        render_tuesday_edit_page(canvas, seq, engine, current_page=0, selected_slot=1)
    elif page == "stochastic-edit":
        seq, engine = _make_stochastic_data()
        render_stochastic_edit_page(canvas, seq, engine)
    elif page == "stochastic-steps":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps(canvas, seq, engine, layer_bank=0, selected_step=5, section=0)
    elif page == "stochastic-dice":
        seq, engine = _make_stochastic_data()
        render_stochastic_dice(canvas, seq, engine, axis=0, selected_steps={5, 6, 7}, section=0)
    elif page == "stochastic-pitch":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch(canvas, seq, engine, slot=0, selected_degree=2)
    elif page == "stochastic-marbles":
        seq, engine = _make_stochastic_data()
        render_stochastic_marbles(canvas, seq, engine, slot=0)
    elif page == "stochastic-lock":
        seq, engine = _make_stochastic_data()
        render_stochastic_lock(canvas, seq, engine, slot=0, selected_event=7)
    elif page == "stochastic-track":
        seq, engine = _make_stochastic_data()
        render_stochastic_track(canvas, seq, engine, current_page=0, selected_slot=1)
    elif page == "stochastic-steps-skylines":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_skylines(canvas, seq, engine, section=0)
    elif page == "stochastic-steps-delinquencer":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_delinquencer(canvas, seq, engine, section=0)
    elif page == "stochastic-steps-euclid":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_euclid(canvas, seq, engine, section=0)
    elif page == "stochastic-pitch-prob-melod":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch_prob_melod(canvas, seq, engine)
    elif page == "stochastic-pitch-circle":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch_circle(canvas, seq, engine, selected_degree=2)
    elif page == "stochastic-pitch-less-concepts":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch_less_concepts(canvas, seq, engine, selected_degree=2)
    elif page == "stochastic-dice-shredder":
        seq, engine = _make_stochastic_data()
        render_stochastic_dice_shredder(canvas, seq, engine, axis=0, active_cell=5)
    elif page == "stochastic-dice-register":
        seq, engine = _make_stochastic_data()
        render_stochastic_dice_register(canvas, seq, engine, axis=0)
    elif page == "stochastic-marbles-rndwalk":
        seq, engine = _make_stochastic_data()
        render_stochastic_marbles_rndwalk(canvas, seq, engine, slot=0)
    elif page == "stochastic-marbles-constellations":
        seq, engine = _make_stochastic_data()
        render_stochastic_marbles_constellations(canvas, seq, engine, slot=0)
    elif page == "stochastic-lock-skylines":
        seq, engine = _make_stochastic_data()
        render_stochastic_lock_skylines(canvas, seq, engine)
    elif page == "stochastic-lock-qfwfq":
        seq, engine = _make_stochastic_data()
        render_stochastic_lock_qfwfq(canvas, seq, engine)
    elif page == "stochastic-track-rndwalk":
        seq, engine = _make_stochastic_data()
        render_stochastic_track_rndwalk(canvas, seq, engine, current_page=0, selected_slot=0)
    elif page == "stochastic-track-sequencex":
        seq, engine = _make_stochastic_data()
        render_stochastic_track_sequencex(canvas, seq, engine, current_page=0, selected_slot=0)
    elif page == "stochastic-steps-bline":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_bline(canvas, seq, engine, section=0)
    elif page == "stochastic-steps-hiswing":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_hiswing(canvas, seq, engine, section=0)
    elif page == "stochastic-steps-meadowphysics":
        seq, engine = _make_stochastic_data()
        render_stochastic_steps_meadowphysics(canvas, seq, engine, section=0)
    elif page == "stochastic-pitch-kreislauf":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch_kreislauf(canvas, seq, engine, selected_degree=2)
    elif page == "stochastic-pitch-bline":
        seq, engine = _make_stochastic_data()
        render_stochastic_pitch_bline(canvas, seq, engine, selected_degree=2)
    elif page == "stochastic-dice-grd":
        seq, engine = _make_stochastic_data()
        render_stochastic_dice_grd(canvas, seq, engine, axis=0)
    elif page == "stochastic-dice-kreislauf":
        seq, engine = _make_stochastic_data()
        render_stochastic_dice_kreislauf(canvas, seq, engine, axis=0)
    elif page == "stochastic-marbles-pit-orchisstra":
        seq, engine = _make_stochastic_data()
        render_stochastic_marbles_pit_orchisstra(canvas, seq, engine, slot=0)
    elif page == "stochastic-marbles-bline":
        seq, engine = _make_stochastic_data()
        render_stochastic_marbles_bline(canvas, seq, engine, slot=0)
    elif page == "stochastic-lock-meadowphysics":
        seq, engine = _make_stochastic_data()
        render_stochastic_lock_meadowphysics(canvas, seq, engine)
    elif page == "stochastic-lock-pit-orchisstra":
        seq, engine = _make_stochastic_data()
        render_stochastic_lock_pit_orchisstra(canvas, seq, engine)
    elif page == "stochastic-track-bline":
        seq, engine = _make_stochastic_data()
        render_stochastic_track_bline(canvas, seq, engine, current_page=0, selected_slot=0)
    elif page == "stochastic-track-pitter-patter":
        seq, engine = _make_stochastic_data()
        render_stochastic_track_pitter_patter(canvas, seq, engine, current_page=0, selected_slot=0)
    elif page == "ref-prob-melod":
        render_ref_prob_melod(canvas)
    elif page == "ref-shredder":
        render_ref_shredder(canvas)
    elif page == "ref-euclid":
        render_ref_euclid(canvas)
    elif page == "ref-delinquencer":
        render_ref_delinquencer(canvas)
    elif page == "ref-skylines":
        render_ref_skylines(canvas)
    elif page == "ref-prob-div":
        render_ref_prob_div(canvas)
    # Modulator page — current vs proposed design
    elif page == "modulator-current-sine":
        render_modulator_current_sine(canvas)
    elif page == "modulator-current-adsr":
        render_modulator_current_adsr(canvas)
    elif page == "modulator-current-random":
        render_modulator_current_random(canvas)
    elif page == "modulator-current-chaos":
        render_modulator_current_chaos(canvas)
    elif page == "modulator-list-sine-proposed":
        render_modulator_list_sine(canvas)
    elif page == "modulator-list-adsr-proposed":
        render_modulator_list_adsr(canvas)
    elif page == "modulator-list-random-proposed":
        render_modulator_list_random(canvas)
    elif page == "modulator-list-random-uniform-proposed":
        render_modulator_list_random_uniform(canvas)
    elif page == "modulator-list-spring-proposed":
        render_modulator_list_spring(canvas)
    elif page == "modulator-list-spring-p2-proposed":
        render_modulator_list_spring_p2(canvas)
    elif page == "modulator-list-chaos-p2-proposed":
        render_modulator_list_chaos_p2(canvas)
    elif page == "modulator-list-chaos-p1-proposed":
        render_modulator_list_chaos_p1(canvas)
    elif page == "modulator-list-adsr-p2-proposed":
        render_modulator_list_adsr_p2(canvas)
    elif page == "modulator-list-adsr-p1-proposed":
        render_modulator_list_adsr_p1(canvas)
    elif page == "modulator-list-lfo-proposed":
        render_modulator_list_lfo(canvas)
    elif page == "modulator-list-chaos-proposed":
        render_modulator_list_chaos(canvas)
    elif page == "modulator-proposed-sine":
        render_modulator_proposed_sine(canvas)
    elif page == "modulator-proposed-adsr":
        render_modulator_proposed_adsr(canvas)
    elif page == "modulator-proposed-random":
        render_modulator_proposed_random(canvas)
    elif page == "modulator-proposed-chaos":
        render_modulator_proposed_chaos(canvas)
    elif page == "modulator-proposed-adsr-page2":
        render_modulator_proposed_adsr_page2(canvas)
    elif page == "modulator-proposed-chaos-page2":
        render_modulator_proposed_chaos_page2(canvas)
    elif page == "modulator-rate-free-proposed":
        render_modulator_rate_free_proposed(canvas)
    elif page == "modulator-rate-tempo-proposed":
        render_modulator_rate_tempo_proposed(canvas)
    elif page == "modulator-gatemodes-proposed":
        render_modulator_gatemodes_proposed(canvas)
    elif page == "modulator-scope-proposed":
        render_modulator_scope_proposed(canvas)
    elif page == "modulator-destinations-grid-proposed":
        render_modulator_destinations_grid_proposed(canvas)
    elif page == "modulator-justf-time":
        render_modulator_justf_time(canvas)
    elif page == "modulator-justf-intone":
        render_modulator_justf_intone(canvas)
    elif page == "modulator-justf-follower":
        render_modulator_justf_follower(canvas)
    elif page == "modulator-justf-adsr-follower":
        render_modulator_justf_adsr_follower(canvas)
    elif page == "modulator-justf-adsr-intone":
        render_modulator_justf_adsr_intone(canvas)
    elif page == "modulator-geode-globals":
        render_modulator_geode_globals(canvas)
    elif page == "modulator-geode-voice":
        render_modulator_geode_voice(canvas)
    elif page == "modulator-adsr-floor":
        render_modulator_adsr_floor(canvas)
    elif page == "stochastic-loop-proposed":
        render_stochastic_loop_proposed(canvas)
    elif page == "stochastic-loop-proposed-held":
        render_stochastic_loop_proposed(canvas, held_step=14)
    elif page == "stochastic-live-proposed":
        render_stochastic_live_proposed(canvas)
    elif page == "stochastic-live-proposed-held":
        render_stochastic_live_proposed(canvas, held_step=9)
    elif page == "stochastic-live-proposed-burst":
        render_stochastic_live_proposed(canvas, burst=80, burst_count=6, burst_rate=80, variation=60)
    elif page == "stochastic-live-proposed-spread":
        render_stochastic_live_proposed(canvas, spread=90, bias=30, contour=-50)
    elif page == "stochastic-live-constellation":
        render_stochastic_live_constellation(canvas)
    elif page == "stochastic-live-constellation-burst":
        render_stochastic_live_constellation(canvas, burst=80, burst_count=6, burst_rate=80,
                                             variation=60, playing_idx=3)
    elif page == "stochastic-live-constellation-complex":
        render_stochastic_live_constellation(canvas, complexity=90, contour=-40,
                                             variation=70, duration=6)
    elif page == "stochastic-live-constellation-newseed":
        render_stochastic_live_constellation(canvas, seed=0xBADD00D5, playing_idx=7)
    elif page == "routing-list":
        from pages_routing import render_routing_list
        render_routing_list(canvas, selected=0, scroll=0)
    elif page == "routing-list-mid":
        from pages_routing import render_routing_list
        render_routing_list(canvas, selected=9, scroll=7)
    elif page == "routing-list-end":
        from pages_routing import render_routing_list
        render_routing_list(canvas, selected=13, scroll=10)
    elif page == "routing-hero":
        from pages_routing_hero import render_routing_hero
        render_routing_hero(canvas, track=3, mode="NOTE", topic_idx=0, selected_row=0)
    elif page == "routing-hero-step":
        from pages_routing_hero import render_routing_hero
        render_routing_hero(canvas, track=3, mode="NOTE", topic_idx=1, selected_row=1)
    elif page == "routing-scope":
        from pages_routing_scope import render_routing_scope
        render_routing_scope(canvas, selected=(2, 4), cursor=2)
    elif page == "routing-unified":
        from pages_routing_unified import render_routing_unified
        render_routing_unified(canvas, tab_idx=0, selected_row=0)
    elif page == "routing-unified-clock":
        from pages_routing_unified import render_routing_unified
        render_routing_unified(canvas, tab_idx=1, selected_row=0)
    elif page == "routing-unified-idx":
        from pages_routing_unified import render_routing_unified
        render_routing_unified(canvas, tab_idx=3, selected_row=0, engine="indexed")
    elif page == "routing-unified-idx1":
        from pages_routing_unified import render_routing_unified
        render_routing_unified(canvas, tab_idx=3, selected_row=0, engine="indexed1")
    elif page == "routing-spread":
        from pages_routing_spread import render_routing_spread
        render_routing_spread(canvas, selected_row=0)
    elif page == "routing-target":
        from pages_routing_target import render_routing_target
        render_routing_target(canvas, selected_row=0)
    elif page == "routing-scope-bytype":
        from pages_routing_scope import render_routing_scope
        render_routing_scope(canvas, selected=(0,2,4), cursor=2)
    elif page == "routing-scope-global":
        from pages_routing_scope import render_routing_scope
        render_routing_scope(canvas, selected=(), cursor=0)
    elif page == "routing-source":
        from pages_routing_source import render_routing_source
        render_routing_source(canvas, selected=1, scroll=0)
    elif page == "routing-source-gate":
        from pages_routing_source import render_routing_source
        render_routing_source(canvas, selected=24, scroll=19)
    elif page == "routing-tabscope":
        from pages_routing_tabscope import render_routing_tabscope
        render_routing_tabscope(canvas, scenario=1)
    elif page == "routing-tabscope-bytype":
        from pages_routing_tabscope import render_routing_tabscope
        render_routing_tabscope(canvas, scenario=2)
    elif page == "routing-depth":
        from pages_routing_depth import render_routing_depth
        render_routing_depth(canvas, depth=45, absolute=False, source_pos=0.6)
    elif page == "routing-depth-abs":
        from pages_routing_depth import render_routing_depth
        render_routing_depth(canvas, depth=70, absolute=True, source_pos=0.5)
    elif page == "routing-tablane":
        from pages_routing_tablane import render_routing_tablane
        render_routing_tablane(canvas, scenario=1)
    elif page == "routing-tab-global":
        from pages_routing_tabeditor import render_routing_tabeditor
        render_routing_tabeditor(canvas)
    elif page == "routing-tab-clock":
        from pages_routing_tabeditor import render_routing_tabeditor_clock
        render_routing_tabeditor_clock(canvas)
    elif page == "routing-tab-global-source":
        from pages_routing_tabeditor import render_routing_tabeditor_source
        render_routing_tabeditor_source(canvas)
    elif page == "routing-tab-shaper":
        from pages_routing_tabeditor import render_routing_tabeditor_shaper
        render_routing_tabeditor_shaper(canvas)
    elif page == "routing-bus":
        from pages_routing_tabeditor import render_routing_bus
        render_routing_bus(canvas)
    elif page == "routing-door":
        from pages_routing_tabeditor import render_routing_door
        render_routing_door(canvas)
    elif page == "routing-rowbar":
        from pages_routing_rowbar import render_routing_rowbar
        render_routing_rowbar(canvas, scenario=1)
    elif page == "mod-param-row":
        from pages_modulation import render_mod_param_row
        render_mod_param_row(canvas)
    elif page == "mod-param-depth":
        from pages_modulation import render_mod_param_depth
        render_mod_param_depth(canvas)
    elif page == "mod-spread":
        from pages_modulation import render_mod_spread
        render_mod_spread(canvas)
    elif page == "mod-source":
        from pages_modulation import render_mod_source
        render_mod_source(canvas)
    elif page == "mod-matrix":
        from pages_modulation import render_mod_matrix
        render_mod_matrix(canvas)
    elif page == "mod-matrix-bytype":
        from pages_modulation import render_mod_matrix_bytype
        render_mod_matrix_bytype(canvas)
    elif page == "mod-matrix-src":
        from pages_modulation import render_mod_matrix_src
        render_mod_matrix_src(canvas)
    elif page == "bus":
        from pages_bus import render_bus
        render_bus(canvas)
    elif page == "bus-grid-proposed":
        from pages_bus import render_bus_grid_proposed
        render_bus_grid_proposed(canvas)
    elif page == "engine-curve-proposed":
        from pages_engine import render_engine_curve
        render_engine_curve(canvas)
    elif page == "engine-note-proposed":
        from pages_engine import render_engine_note
        render_engine_note(canvas)
    elif page == "engine-curve-shaper-proposed":
        from pages_engine import render_engine_curve_shaper
        render_engine_curve_shaper(canvas)
    elif page == "engine-curve-scale-proposed":
        from pages_engine import render_engine_curve_scale
        render_engine_curve_scale(canvas)
    elif page == "bus-nav-proposed":
        from pages_bus import render_bus_nav_proposed
        render_bus_nav_proposed(canvas)
    elif page == "bus-edit-source-proposed":
        from pages_bus import render_bus_edit_source_proposed
        render_bus_edit_source_proposed(canvas)
    elif page == "bus-edit-depth-proposed":
        from pages_bus import render_bus_edit_depth_proposed
        render_bus_edit_depth_proposed(canvas)
    elif page == "midi-proposed":
        from pages_midi import render_midi
        render_midi(canvas)
    elif page == "midi-empty-proposed":
        from pages_midi import render_midi_empty
        render_midi_empty(canvas)
    elif page == "midi-edit-proposed":
        from pages_midi import render_midi_edit
        render_midi_edit(canvas)
    elif page == "midi-learn-proposed":
        from pages_midi import render_midi_learn
        render_midi_learn(canvas)
    else:
        raise ValueError(f"Unknown page: {page}")

    return framebuffer_to_image(fb, scale=scale)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description='Generate OLED UI preview images')
    parser.add_argument('-o', '--output', default='preview.png', help='Output image path')
    parser.add_argument('--scale', type=int, default=4, help='Pixel upscale factor (default 4)')
    parser.add_argument('--page', required=True,
                        choices=['note-edit', 'discrete-map-edit', 'tuesday-edit', 'stochastic-edit',
                                 # Primary stochastic pages
                                 'stochastic-steps', 'stochastic-dice', 'stochastic-pitch',
                                 'stochastic-marbles', 'stochastic-lock', 'stochastic-track',
                                 # Hybrid reference-crossover pages
                                 'stochastic-steps-skylines', 'stochastic-steps-delinquencer', 'stochastic-steps-euclid',
                                 'stochastic-pitch-prob-melod', 'stochastic-pitch-circle', 'stochastic-pitch-less-concepts',
                                 'stochastic-dice-shredder', 'stochastic-dice-register',
                                 'stochastic-marbles-rndwalk', 'stochastic-marbles-constellations',
                                 'stochastic-lock-skylines', 'stochastic-lock-qfwfq',
                                 'stochastic-track-rndwalk', 'stochastic-track-sequencex',
                                 # Norns-inspired hybrid pages (second wave)
                                 'stochastic-steps-bline', 'stochastic-steps-hiswing', 'stochastic-steps-meadowphysics',
                                 'stochastic-pitch-kreislauf', 'stochastic-pitch-bline',
                                 'stochastic-dice-grd', 'stochastic-dice-kreislauf',
                                 'stochastic-marbles-pit-orchisstra', 'stochastic-marbles-bline',
                                 'stochastic-lock-meadowphysics', 'stochastic-lock-pit-orchisstra',
                                 'stochastic-track-bline', 'stochastic-track-pitter-patter',
                                 # Routing UI redesign
                                 'routing-list', 'routing-list-mid', 'routing-list-end',
                                 'routing-hero', 'routing-hero-step', 'routing-scope', 'routing-scope-global', 'routing-unified', 'routing-unified-clock', 'routing-spread', 'routing-target', 'routing-scope-bytype', 'routing-unified-idx', 'routing-unified-idx1', 'routing-source', 'routing-source-gate', 'routing-tabscope', 'routing-tabscope-bytype', 'routing-depth', 'routing-depth-abs', 'routing-tablane', 'routing-rowbar',
                                 'routing-tab-global', 'routing-tab-clock', 'routing-tab-global-source', 'routing-tab-shaper', 'routing-bus', 'routing-door',
                                 'mod-param-row', 'mod-param-depth', 'mod-spread', 'mod-source', 'mod-matrix', 'mod-matrix-bytype', 'mod-matrix-src',
                                 'bus', 'engine-curve-proposed', 'engine-note-proposed', 'bus-grid-proposed', 'bus-nav-proposed', 'bus-edit-source-proposed', 'bus-edit-depth-proposed',
                                 'midi-proposed', 'midi-empty-proposed', 'midi-edit-proposed', 'midi-learn-proposed',
                                 'engine-curve-shaper-proposed', 'engine-curve-scale-proposed',
                                 # Pure reference screens
                                 'ref-prob-melod', 'ref-shredder', 'ref-euclid',
                                 'ref-delinquencer', 'ref-skylines', 'ref-prob-div',
                                 # Modulator page designs
                                 'modulator-current-sine', 'modulator-current-adsr',
                                 'modulator-current-random', 'modulator-current-chaos',
                                 'modulator-proposed-sine', 'modulator-proposed-adsr',
                                 'modulator-proposed-random', 'modulator-proposed-chaos',
                                 'modulator-proposed-adsr-page2', 'modulator-proposed-chaos-page2',
                                 'modulator-rate-free-proposed', 'modulator-rate-tempo-proposed', 'modulator-gatemodes-proposed', 'modulator-scope-proposed', 'modulator-destinations-grid-proposed', 'modulator-justf-time', 'modulator-justf-intone', 'modulator-justf-follower', 'modulator-justf-adsr-follower', 'modulator-justf-adsr-intone', 'modulator-geode-globals', 'modulator-geode-voice', 'modulator-adsr-floor',
                                 'modulator-list-sine-proposed', 'modulator-list-adsr-proposed', 'modulator-list-random-proposed', 'modulator-list-chaos-proposed', 'modulator-list-random-uniform-proposed', 'modulator-list-spring-proposed', 'modulator-list-spring-p2-proposed', 'modulator-list-chaos-p2-proposed', 'modulator-list-chaos-p1-proposed', 'modulator-list-adsr-p2-proposed', 'modulator-list-adsr-p1-proposed', 'modulator-list-lfo-proposed',
                                 'stochastic-loop-proposed', 'stochastic-loop-proposed-held',
                                 'stochastic-live-proposed', 'stochastic-live-proposed-held',
                                 'stochastic-live-proposed-burst', 'stochastic-live-proposed-spread',
                                 'stochastic-live-constellation', 'stochastic-live-constellation-burst',
                                 'stochastic-live-constellation-complex', 'stochastic-live-constellation-newseed'],
                        help='Which edit page or reference screen to render')
    args = parser.parse_args()

    img = render_page(args.page, scale=args.scale)
    out_path = os.path.abspath(args.output)
    img.save(out_path)
    print(f"Saved {args.page} preview to {out_path}")


if __name__ == '__main__':
    main()
