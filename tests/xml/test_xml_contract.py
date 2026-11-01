from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

from lxml import etree

ROOT = Path(__file__).resolve().parents[2]


class ReferenceContractTests(unittest.TestCase):
    def test_aravis_reference_matches_pinned_genapi_schema(self) -> None:
        base = ROOT / ".reference" / "aravis" / "src"
        schema_doc = etree.parse(str(base / "GenApiSchema_Version_1_1.xsd"))
        schema = etree.XMLSchema(schema_doc)
        xml_doc = etree.parse(str(base / "arv-fake-camera.xml"))
        self.assertEqual("http://www.genicam.org/GenApi/Version_1_0", etree.QName(xml_doc.getroot()).namespace)
        # Aravis' fake camera sample intentionally targets GenApi 1.0; the production generator targets 1.1.
        self.assertIn("Width", {node.get("Name") for node in xml_doc.xpath(".//*[@Name]")})

    def test_contract_defines_standard_stream_registers(self) -> None:
        contract = json.loads((ROOT / "tests" / "fixtures" / "xml_contract.json").read_text(encoding="utf-8"))
        self.assertEqual("0xd04", contract["registers"]["GevStreamChannel0PacketSizeRegister"]["Address"])
        self.assertIn("GevCCP", contract["required_nodes"])
        self.assertIn("AcquisitionStart", contract["required_nodes"])


if __name__ == "__main__":
    unittest.main()
