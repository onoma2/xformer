"""
Hybrid stochastic page renderers.
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
from pages_bline import (
    render_stochastic_marbles_bline,
    render_stochastic_pitch_bline,
    render_stochastic_steps_bline,
    render_stochastic_track_bline,
)

from pages_circle import (
    render_stochastic_pitch_circle,
)

from pages_constellations import (
    render_stochastic_marbles_constellations,
)

from pages_delinquencer import (
    render_stochastic_steps_delinquencer,
)

from pages_euclid import (
    render_stochastic_steps_euclid,
)

from pages_grd import (
    render_stochastic_dice_grd,
)

from pages_hiswing import (
    render_stochastic_steps_hiswing,
)

from pages_kreislauf import (
    render_stochastic_dice_kreislauf,
    render_stochastic_pitch_kreislauf,
)

from pages_less_concepts import (
    render_stochastic_pitch_less_concepts,
)

from pages_meadowphysics import (
    render_stochastic_lock_meadowphysics,
    render_stochastic_steps_meadowphysics,
)

from pages_pit_orchisstra import (
    render_stochastic_lock_pit_orchisstra,
    render_stochastic_marbles_pit_orchisstra,
)

from pages_pitter_patter import (
    render_stochastic_track_pitter_patter,
)

from pages_prob_melod import (
    render_stochastic_pitch_prob_melod,
)

from pages_qfwfq import (
    render_stochastic_lock_qfwfq,
)

from pages_register import (
    render_stochastic_dice_register,
)

from pages_rndwalk import (
    render_stochastic_marbles_rndwalk,
    render_stochastic_track_rndwalk,
)

from pages_sequencex import (
    render_stochastic_track_sequencex,
)

from pages_shredder import (
    render_stochastic_dice_shredder,
)

from pages_skylines import (
    render_stochastic_lock_skylines,
    render_stochastic_steps_skylines,
)

