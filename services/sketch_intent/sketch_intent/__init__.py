"""Sketch Intent Resolver (PlatypusOne issue #37).

Turns an imperfect measured 2D contour into a reviewable sketch proposal:
evidence contour -> primitive hypotheses -> constraint hypotheses ->
proposed geometry -> human review -> export.

The evidence is never modified. Every inferred line, arc, circle and
constraint carries its fit residuals and the size of the correction it
implies, and nothing becomes exportable geometry without review.
Residuals are fit diagnostics, not measurement accuracy.
"""

# Bump on any change that can alter a proposal for the same input.
RESOLVER_VERSION = "sketch-intent-0.1.0"
PROPOSAL_SCHEMA = "platypus.sketch_proposal/1"
