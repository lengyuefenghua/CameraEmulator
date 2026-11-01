from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from lxml import etree


def validate(xml_path: Path, schema_path: Path, contract_path: Path | None) -> None:
    schema_doc = etree.parse(str(schema_path))
    schema = etree.XMLSchema(schema_doc)
    document = etree.parse(str(xml_path))
    schema.assertValid(document)

    if contract_path is None:
        print(f"XSD valid: {xml_path}")
        return

    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    ns = {"g": contract["namespace"]}
    root = document.getroot()
    actual = {node.get("Name"): node for node in root.xpath(".//*[@Name]")}

    missing = sorted(set(contract["required_nodes"]) - actual.keys())
    if missing:
        raise ValueError(f"Missing required GenApi nodes: {', '.join(missing)}")

    for name, expected in contract.get("registers", {}).items():
        node = actual.get(name)
        if node is None:
            raise ValueError(f"Reference register node not present: {name}")
        fields = {child.tag.rsplit("}", 1)[-1]: (child.text or "").strip() for child in node}
        for field, value in expected.items():
            if fields.get(field) != str(value):
                raise ValueError(f"{name}.{field}: expected {value!r}, got {fields.get(field)!r}")

    print(f"XSD and contract valid: {xml_path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml", type=Path)
    parser.add_argument("--schema", type=Path, required=True)
    parser.add_argument("--contract", type=Path)
    args = parser.parse_args()
    try:
        validate(args.xml, args.schema, args.contract)
        return 0
    except (etree.XMLSyntaxError, etree.DocumentInvalid, OSError, ValueError, KeyError) as exc:
        print(f"XML validation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
