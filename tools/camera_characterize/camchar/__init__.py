"""Camera characterization: evidence contract, analysis and decision gate (#40).

Analysis results are only as good as the physical captures behind them.
Synthetic fixtures exercise the software; they are never camera evidence.
"""

# Bump on any change that can alter a computed number. Every derived file and
# report carries it, so a metric always traces to the code that produced it.
ANALYSIS_VERSION = "camchar-0.2.0"

UNKNOWN = "UNKNOWN"
