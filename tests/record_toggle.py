"""Finalize two recordings without interrupting the source video."""
import json, pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory)
    source=root/'source.mkv'
    subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=160x120:rate=10','-t','15','-c:v','libx264','-threads','1','-g','10','-bf','0',str(source)],check=True)
    subprocess.run([sys.argv[1],str(source),'--record-toggle',directory],check=True,timeout=20)
    for index in range(2):
        clip=root/f'clip{index}.mkv'
        result=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-count_frames','-show_entries','stream=codec_name,nb_read_frames','-of','json',str(clip)]))
        assert result['streams'][0]['codec_name']=='h264'
        assert int(result['streams'][0]['nb_read_frames'])>0
    if len(sys.argv)>2:
        subprocess.run([sys.argv[2],str(source)],check=True,timeout=30)
print('Two start/stop cycles finalized playable clips while source playback continued.')
