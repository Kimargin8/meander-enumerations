"""Summarize a completed multi-order calculation, counting each terminal height once."""
from argparse import ArgumentParser
import json
from pathlib import Path
from campaign import atomic,digest
from port_evidence import verify


def main():
    p=ArgumentParser(description=__doc__)
    p.add_argument("folder",type=Path);p.add_argument("n",type=int)
    a=p.parse_args();base=a.folder;n=a.n
    total=json.loads((base/"assembled"/f"n{n:02}.json").read_bytes())
    sectors=[];external=[]
    for h in range(n%2,n+1,2):
        folder=base/"all_orders"/f"n{n:02}h{h:02}"
        final=folder/f"n{n:02}.json"
        expected=total["by_height"][str(h)]["value"]
        if not final.exists():
            identity=base/"identities"/f"n{n:02}"/f"h{h:02}.json"
            r=json.loads(identity.read_bytes());assert r["value"]==expected
            external.append(dict(h=h,value=expected,source_sha256=digest(identity)));continue
        r=json.loads(final.read_bytes());assert verify(r) and r["value"]==expected
        intervals=[]
        for f in sorted(folder.glob("n*.json")):
            seg=json.loads(f.read_bytes())
            if "wall_seconds_this_invocation" not in seg:continue
            intervals.append(dict(source_layer=seg["resumed_from_layer"],target_layer=seg["t"],
                             wall_seconds=seg["wall_seconds_this_invocation"],
                             process_peak_KiB=seg["process_peak_KiB_this_invocation"]))
        sectors.append(dict(h=h,value=expected,peak_states=r["peak_states_through_layer"],
                   attempted_moves=r["attempted_moves_through_layer"],layer_io_bytes=r["layer_bytes_read_and_written"],
                   max_two_layers_bytes=r["max_two_layers_bytes"],evidence_sha256=digest(final),
                   measured_segments=intervals,uses_lift="lifted_seed" in r,packed=r.get("packed_storage",False)))
    assert sum(q["value"] for q in sectors+external)==total["D"]
    result=dict(n=n,K_index=total["K_index"],K=total["K"],D=total["D"],counted_heights=len(sectors),
        identity_heights=external,attempted_moves_across_counted_heights=sum(x["attempted_moves"] for x in sectors),
        layer_input_and_output_across_counted_heights_bytes=sum(x["layer_io_bytes"] for x in sectors),
        largest_frontier=max(x["peak_states"] for x in sectors),
        peak_two_layers_one_height_bytes=max(x["max_two_layers_bytes"] for x in sectors),
        recorded_process_peak_KiB=max(i["process_peak_KiB"] for x in sectors for i in x["measured_segments"]),
        sum_of_recorded_segment_wall_seconds=sum(i["wall_seconds"] for x in sectors for i in x["measured_segments"]),
        measured_time_scope="Stored successful intervals only. Intervals can overlap in real time. Earlier runs, partial restarts and independent checks have separate costs.",
        work_scope="Each terminal height counted once. Lifted work includes the reusable shorter-prefix work once and the complementary transfer.",
        sectors=sectors)
    simultaneous=base/"terminal_resources.json"
    if simultaneous.exists():result["concurrent_sampling"]=json.loads(simultaneous.read_bytes())
    atomic(base/f"terminal_report{n}.json",result)
    print(n,result["K"],result["largest_frontier"],result["peak_two_layers_one_height_bytes"])


if __name__=="__main__":main()
