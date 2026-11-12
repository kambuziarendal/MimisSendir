FROM codeberg.org/slidge/slidgnal:v0.5.1-amd64@sha256:ea4aaf58ad5eab65214ebfb43bfe4178b30baa90251c78f54b57e1cd79756982
LABEL org.opencontainers.image.title="Renga pinned Signal bridge candidate"
LABEL org.opencontainers.image.description="Offline-tested Python stability overlay; live pilot authorization required"
COPY slidgnal/ /venv/lib/python3.13/site-packages/slidgnal/
COPY slidge/ /venv/lib/python3.13/site-packages/slidge/
# Parse every modified module without writing caches or starting the gateway.
RUN /venv/bin/python -c "import ast,pathlib; root=pathlib.Path('/venv/lib/python3.13/site-packages'); [ast.parse((root/p).read_text()) for p in ['slidgnal/session.py','slidgnal/group.py','slidgnal/contact.py','slidgnal/mixins.py','slidgnal/event_dispatch.py','slidgnal/native_calls.py','slidgnal/bounded_map.py','slidge/group/room.py']]"
