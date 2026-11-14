Grauman's main and forecourt wing final input handoff

Both inputs are frozen and ready for root's texture-only work:
- work/LM_Graumans_input.glb,1924triangles,82x58x16m.
- work/LM_Graumans_ForecourtWall_input.glb,190triangles,6x20x5m, suitable for mirrored use.

Sources src/hub_egyptian/<id>.blend contain the exact named mesh and one M_<id> material. Builder build_graumans.py uses the reviewed Mesh/UV helper definitions in build.py via AST extraction, without executing the Egyptian export loop. Keep both scripts for rebuilds. The builder never modifies old Egyptian or shelter GLB inputs.

Root generated and reviewed the concepts and historical research in src/hub_root before these models were authored. No new image generation or Rodin calls were made for these two assets by this subagent. Central README/manifest/ledger files remain untouched.

The massive theatre body occupies its exact collision footprint, with front at-Y=-29m. The central entry wall is recessed2m toY=-27. The broad pagoda is deliberately compressed to fit16m height, with upturned corners and a continuous narrow crown. Two coral-column silhouettes and two seated chunky stone Heaven Dogs remain inside the footprint. Their faces, manes, toes and gold iron-mask details are painted texture instructions. The16m dragon panel above the entry has one continuous UV island, so texture fragments do not repeat. Long wing faces and roof panels intentionally repeat UVs; visible street faces50px/m, roofs/back/side25px/m with16pxrole separation.

The first strict FBX trial caught coincident hidden faces where the crown underside met its support drum. The final builder authors neither buried cap, and segments the drum to the SAME32crown-ring vertices. Main and wall now pass source-position weld, actual asset_io FBX export/import, a further FBX positional weld with orientation checks, and a fully independent frozen-GLB import/weld/asset_io round-trip. All report zero branching edges, zero-area faces, loose vertices or inconsistent shared-edge orientation. No cleanup helper is needed on these final two inputs. Authoritative audits: work/hub_egyptian/graumans_audit.json and graumans_input_audit.json. Intended open ground-facing boundaries remain.

Texture queue work/hub_egyptian/graumans_texture_request_queue.json uses the two original generated34concepts as references and distinct main-versus-wall prompts. Perasset metadata includes source/nominal/bounds/UVroles/sign attachment coordinates. Blank front sign cards require root's correctly spelled OFL lettering. Clay process renders: work/hub_egyptian/<id>_{front34,back34,top}_clay.png. Root produces the final textured phase previews and manifest entries.
