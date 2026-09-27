import sys
import unittest
from unittest.mock import patch

from backend.intent_engine import IntentProjectionEngine, _generate_semantic_embedding
from backend import main


class IntentEngineRegressionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.engine = IntentProjectionEngine()
        self.engine.openai_api_key = ""

    def test_or_inside_word_does_not_trigger_ambiguity(self) -> None:
        result = self.engine.process_intent("Please format the deployment configuration")
        self.assertFalse(result["is_ambiguous"])

    def test_fallback_embedding_is_repeatable(self) -> None:
        self.assertEqual(
            _generate_semantic_embedding("repeatable projection"),
            _generate_semantic_embedding("repeatable projection"),
        )


class MainConfigurationRegressionTests(unittest.TestCase):
    def test_main_passes_requested_host_to_uvicorn(self) -> None:
        with patch.object(sys, "argv", ["main.py", "--host", "127.0.0.1"]), patch(
            "uvicorn.run"
        ) as run:
            main.main()

        self.assertEqual(run.call_args.kwargs["host"], "127.0.0.1")


if __name__ == "__main__":
    unittest.main()
