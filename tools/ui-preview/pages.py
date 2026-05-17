"""
Individual edit page renderers.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from canvas import BlendMode, Canvas, Color, Font
from painters import PageWidth, PageHeight, HeaderHeight, FooterHeight, WindowPainter, SequencePainter
from tracks import (
    DiscreteMapSequence, DiscreteMapTrackEngine,
    NoteSequence, NoteTrackEngine,
    TuesdaySequence, TuesdayTrackEngine,
    StochasticSequence, StochasticTrackEngine,
    _ALGO_NAMES,
)

from pages_core import (
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
)

from pages_reference import (
    render_ref_prob_melod,
    render_ref_shredder,
    render_ref_euclid,
    render_ref_delinquencer,
    render_ref_skylines,
    render_ref_prob_div,
)

from pages_hybrid import (
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
)

