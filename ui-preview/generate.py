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
                                 # Pure reference screens
                                 'ref-prob-melod', 'ref-shredder', 'ref-euclid',
                                 'ref-delinquencer', 'ref-skylines', 'ref-prob-div'],
                        help='Which edit page or reference screen to render')
    args = parser.parse_args()

    img = render_page(args.page, scale=args.scale)
    out_path = os.path.abspath(args.output)
    img.save(out_path)
    print(f"Saved {args.page} preview to {out_path}")


if __name__ == '__main__':
    main()
