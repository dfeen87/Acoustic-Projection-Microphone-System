import unittest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)

class TestIntentEndpoints(unittest.TestCase):
    def test_intent_process_endpoint(self):
        response = client.post(
            "/api/intent/process",
            json={"text": "We need to fix the deployment pipeline ASAP!"}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIn("primary_intent", data)
        self.assertIn("secondary_intents", data)
        self.assertIn("constraints", data)
        self.assertIn("emotional_phase", data)
        self.assertIn("reformulated_message", data)
        self.assertIn("projection_vectors", data)
        self.assertEqual(data["emotional_phase"]["urgency_level"], "high")

    def test_intent_process_personality_and_bilingual(self):
        response = client.post(
            "/api/intent/process",
            json={
                "text": "新しい機能をリリースしましょう ship feature",
                "personality": {
                    "style": "founder_voice",
                    "use_emojis": True,
                    "bilingual_jp": True
                }
            }
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIn("JP/EN preserved", data["reformulated_message"])
        self.assertTrue(data["reformulated_message"].endswith("🚀"))

    def test_intent_process_ambiguity_and_candidate_selection(self):
        # Test vague input triggering ambiguity candidates
        response = client.post(
            "/api/intent/process",
            json={"text": "maybe or something"}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertTrue(data["is_ambiguous"])
        self.assertGreaterEqual(len(data["candidates"]), 2)

        # Test selecting a candidate
        selected_cand_id = data["candidates"][0]["id"]
        response2 = client.post(
            "/api/intent/process",
            json={
                "text": "maybe or something",
                "selected_candidate_id": selected_cand_id
            }
        )
        self.assertEqual(response2.status_code, 200)
        data2 = response2.json()
        self.assertFalse(data2["is_ambiguous"])
        self.assertEqual(len(data2["candidates"]), 0)

    def test_intent_process_empty_input(self):
        response = client.post(
            "/api/intent/process",
            json={"text": "   "}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["primary_intent"], "No input provided")
        self.assertFalse(data["is_ambiguous"])

    def test_or_inside_word_does_not_trigger_ambiguity(self):
        response = client.post(
            "/api/intent/process",
            json={"text": "Please format the deployment configuration"}
        )
        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()["is_ambiguous"])

    def test_intent_project_endpoint(self):
        response = client.post(
            "/api/intent/project",
            json={"text": "Test acoustic projection semantic beamforming"}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIn("dominant_vector", data)
        self.assertEqual(len(data["dominant_vector"]), 8)
        self.assertIn("noise_suppressed_db", data)
        self.assertIn("signal_to_noise_ratio", data)

    def test_intent_phase_endpoint(self):
        response = client.post(
            "/api/intent/phase",
            json={"text": "I am really excited and hopeful about this release!"}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIn("emotional_phase", data)
        self.assertEqual(data["input_text"], "I am really excited and hopeful about this release!")


if __name__ == "__main__":
    unittest.main()
