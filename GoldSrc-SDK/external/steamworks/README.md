# Steamworks headers

The Source SDK headers here come from Valve's commit
`be57e3d01ac7d7aa6a2cacbfef187c0b46616db0`, `src/public/steam`.
Keep their copyright notices and contents intact.

[Source SDK provenance](../../evidence/steamworks-source-sdk-origin-r1.json) and
[reference header provenance](../../evidence/steamworks-reference-origin-r1.json)
record source/reference files and hashes. Verify used interfaces against the target;
header availability does not establish Steam runtime or unused-helper parity.

The added [isteamuser.h](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/be57e3d01ac7d7aa6a2cacbfef187c0b46616db0/src/public/steam/isteamuser.h)
has SHA-256 `2d93e98fdfd50551a2c53f0c3fcb3e90bbe4f135683db7005fc018f8074c36c5`.
Its complete `SteamUser023` declaration supplies the voice interface; only the
used voice methods and target version string are matched, without runtime parity.

The added [isteammatchmaking.h](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/be57e3d01ac7d7aa6a2cacbfef187c0b46616db0/src/public/steam/isteammatchmaking.h)
(SHA-256 `c48961fd70ec3dcfa9ff27db2b6f510ff1517d3547fa89b2dcf1742a686ab9e1`) and
[matchmakingtypes.h](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/be57e3d01ac7d7aa6a2cacbfef187c0b46616db0/src/public/steam/matchmakingtypes.h)
(SHA-256 `6ee55eafcf3d6e833c7544d1204c0e25294fdbb5798ea8a71c345c8cebb428fa`) supply
`SteamMatchMaking009` and `SteamMatchMakingServers002` for the server browser.
