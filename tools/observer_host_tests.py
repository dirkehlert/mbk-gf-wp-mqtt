#!/usr/bin/env python3
import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MYMESH = ROOT / "examples" / "simple_repeater" / "MyMesh.cpp"
UITASK = ROOT / "examples" / "simple_repeater" / "UITask.cpp"
README = ROOT / "README.md"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def cpp_append_json_string(text: str | None) -> str:
    out = '"'
    if text is not None:
        for ch in text:
            code = ord(ch)
            if ch in ('"', "\\"):
                out += "\\" + ch
            elif ch == "\n":
                out += "\\n"
            elif ch == "\r":
                out += "\\r"
            elif ch == "\t":
                out += "\\t"
            elif code < 0x20:
                out += f"\\u{code:04x}"
            else:
                out += ch
    out += '"'
    return out


class ObserverWebApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.mymesh = read(MYMESH)
        cls.uitask = read(UITASK)
        cls.readme = read(README)

    def test_node_name_is_json_escaped(self) -> None:
        status_fn = self.mymesh[
            self.mymesh.index("String MyMesh::buildObserverWebStatusJson() const"):
            self.mymesh.index("String MyMesh::buildObserverWebMonitorJson() const")
        ]
        self.assertIn('String body = "{\\"node\\":";', status_fn)
        self.assertIn("appendJsonString(body, _prefs.node_name);", status_fn)
        self.assertNotIn('body += _prefs.node_name;', status_fn)

    def test_json_string_helper_escapes_control_characters(self) -> None:
        payload = 'MBK "GF" \\ WP\n\r\t' + chr(1)
        encoded = cpp_append_json_string(payload)
        self.assertEqual(json.loads(encoded), payload)
        self.assertIn('\\"GF\\"', encoded)
        self.assertIn("\\\\", encoded)
        self.assertIn("\\n", encoded)
        self.assertIn("\\r", encoded)
        self.assertIn("\\t", encoded)
        self.assertIn("\\u0001", encoded)

    def test_position_endpoint_is_post_only(self) -> None:
        self.assertIn('observer_web_server->on("/api/position", HTTP_POST', self.mymesh)
        self.assertNotIn('observer_web_server->on("/api/position", HTTP_ANY', self.mymesh)
        self.assertNotIn("GET  /api/position", self.readme)
        self.assertIn("POST /api/position", self.readme)

    def test_station_view_mode_blocks_mutating_endpoints_except_savepoint(self) -> None:
        time_route = self._route_block("/api/time")
        pos_route = self._route_block("/api/position")
        sp_route = self._route_block("/api/savepoint")
        self.assertIn("observer_web_sta_running", time_route)
        self.assertIn("request->send(403", time_route)
        self.assertIn("observer_web_sta_running", pos_route)
        self.assertIn("request->send(403", pos_route)
        self.assertNotIn("request->send(403", sp_route)

    def test_web_header_refreshes_status_and_monitor(self) -> None:
        self.assertIn("setInterval(refresh,10000)", self.mymesh)
        self.assertIn("setInterval(refreshMonitor,10000)", self.mymesh)
        self.assertIn('id="nodeTime"', self.mymesh)

    def test_mqtt_screen_has_ap_and_view_mode_controls(self) -> None:
        self.assertIn("toggleObserverWebAp", self.uitask)
        self.assertIn("toggleObserverWebStaView", self.uitask)
        self.assertIn("mqtt on", self.mymesh)
        self.assertIn("web.view status", self.mymesh)

    def test_ui_init_does_not_allocate_version_with_strdup(self) -> None:
        self.assertNotIn("strdup", self.uitask)
        self.assertRegex(self.uitask, r"snprintf\(_version_info, sizeof\(_version_info\),")

    def _route_block(self, path: str) -> str:
        start = self.mymesh.index(f'observer_web_server->on("{path}"')
        next_route = self.mymesh.find('observer_web_server->on("', start + 1)
        if next_route == -1:
            next_route = self.mymesh.find("\n}", start)
        return self.mymesh[start:next_route]


if __name__ == "__main__":
    unittest.main(verbosity=2)
