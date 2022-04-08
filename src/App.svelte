<script>
	import Screen from './Screen.svelte'

	let screenComponent;
	let x1 = 0;
	let y1 = 0;
	let x2 = 0;
	let y2 = 0;
	let screenW = 0
	let screenH = 0;

	let lbx = 0;
	let lby = 0;
	let rtx = 0;
	let rty = 0;

	$: if (x2 > 0){
		lbx = x1;
		rtx = x1 + x2;
	}else{
		lbx = x1+x2;
		rtx = x1;
	}

	$: if (y2 > 0){
		lby = y2;
		rty = y1;
	}else{
		lby = y1;
		rty = y1 + y2;
	}

	let fileDropping = false;
	
	$: outputCmd = `-r ${lbx.toFixed(0)},${lby.toFixed(0)},${rtx.toFixed(0)},${rty.toFixed(0)} -W ${screenW} -H ${screenH}`

	function gotoGithub(){
		window.open('https://github.com/nmelihsensoy/blockevent', '_blank');
	}

	function showDropZone(e){
		console.log(e);
		fileDropping = true;
	}

	function hideDropZone(e){
		fileDropping = false;
	}

	function allowDrag(e) {
		e.dataTransfer.dropEffect = 'copy';
		e.preventDefault();
	}

	function dropHandler(ev) {
		console.log('File(s) dropped');

		ev.stopPropagation();
		ev.preventDefault();
		hideDropZone();

		var url=ev.dataTransfer.getData('text/plain');
		if(url){
			screenComponent = url;
		}else{
			var file = ev.dataTransfer.files[0];
			var reader = new FileReader();
			reader.readAsDataURL(file);
			screenComponent = file;

			reader.onload = function() {
				screenComponent = reader.result;
			};

		}
	}

	function copyToClipboard(){
		navigator.clipboard
			.writeText(outputCmd)
			.then(() => {
				console.log("copied");
				console.log(outputCmd);
			})
			.catch(() => {

			});
	}
</script>
<svelte:window on:dragenter={showDropZone}/>
<div id="dropzone" on:dragenter={allowDrag} on:dragover={allowDrag} on:dragleave={hideDropZone} on:drop={dropHandler} class="overlay dropzone {fileDropping === true ? 'dropzone_show' : 'dropzone_hide'}"></div>
<main>
	<div id="container">
		<div class="floating_menu">
			<div class="header">
				<span class="logo floating_menu_element unselectable_text"><h3>blockevent</h3></span>
				<span on:click={gotoGithub} class="logo floating_menu_element floating_menu_button clearfix"><i class="bicon-github"></i></span>
			</div>
			<span class="floating_menu_element"><i class="bicon-upload floating_menu_element_small_icon"></i>drop anywhere to upload <br></span>
			<span class="floating_menu_element"><i class="bicon-zoom_out_map floating_menu_element_small_icon"></i>use <span class="key_emboss key_emboss_combine_left">SHIFT</span>+<span class="key_emboss key_emboss_combine_right">SCROLL</span> to zoom</span>
			<span class="floating_menu_element"><i class="bicon-create floating_menu_element_small_icon"></i>draw with <span class="key_emboss">LEFT CLICK</span></span>
		</div>
		<Screen bind:screenW={screenW} bind:screenH={screenH} bind:rectStartX={x1} bind:rectStartY={y1} bind:rectW={x2} bind:rectH={y2} bind:srcProp={screenComponent}>
		</Screen>
	</div>
	<footer class="footer">
		<div class="terminal">
			<p class="terminal_element terminal_element_left unselectable_text">
				>_
			</p>
			<p class="terminal_element terminal_code"> 
				<span class="unselectable_text">./blockevent </span>{outputCmd}
			</p>
			<p on:click={copyToClipboard} class="terminal_element terminal_element_right clearfix unselectable_text">
				<i class="bicon-content_copy"></i>
			</p>
		</div>
	</footer>
</main>

<style>
	@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600&display=swap');

	:global(*) {
		box-sizing: border-box;
		padding: 0;
		margin: 0;
	}

	:global(a) {
		all: unset;
	}

	:global(html) {
		scrollbar-face-color: #646464;
		scrollbar-base-color: #646464;
		scrollbar-3dlight-color: #646464;
		scrollbar-highlight-color: #646464;
		scrollbar-track-color: #000;
		scrollbar-arrow-color: #000;
		scrollbar-shadow-color: #646464;
 	}

	:global(::-webkit-scrollbar) { width: 8px; height: 3px;}
	:global(::-webkit-scrollbar-button) {  background-color: #666; }
	:global(::-webkit-scrollbar-track) {  background-color: #646464;}
	:global(::-webkit-scrollbar-track-piece) { background-color: #000;}
	:global(::-webkit-scrollbar-thumb) { height: 20px; background-color: #666; border-radius: 3px;}
	:global(::-webkit-scrollbar-corner) { background-color: #646464;}
	:global(::-webkit-resizer) { background-color: #666;}

	:root{
		background-color : #212529;
		background-image : radial-gradient(#3d4146 1px, transparent 2px);
   		background-size : 30px 30px;
		overflow-y: scroll;
		color: hsl(240, 25%, 95%);
		font-family: "IBM Plex Mono", monospace;
	}

	main {
		text-align: center;
		padding: 1em;
		margin: 0 auto;
		font-family: 'IBM Plex Mono', monospace;
		font-weight: 600;
		color: white;
		font-size: large;
		display: flex;
		flex-direction: column;
		align-items:center;
		justify-content: center;
	}

	.header{
		display: flex;
	}

	.floating_menu{
		position: fixed;
		top: 0;
		left: 0;
	}

	.floating_menu_element{
		display: block;
		padding: 6px;
		margin-top: 6px;
		margin-left: 10px;
		background-color: #000;
		box-shadow: rgba(0, 0, 0, 0.15) 1.95px 1.95px 2.6px;
		width: fit-content;
		font-family: 'IBM Plex Mono', monospace;
		font-weight: 400;
		font-size: small;
	}

	.floating_menu_element_small_icon{
		margin-right: 6px;
	}

	.floating_menu_button{
		display: flex;
		font-size:x-large;
		align-items: center;
  		justify-content: center;
	}

	.floating_menu_button:hover{
		background-color: #212529;
		cursor: pointer; 
	}

	.logo{
		padding: 10px !important;
		margin-top: 10px !important;
		margin-bottom: 6px !important; 
		margin-left: 10px !important;
	}

	.logo h3{
		background: #aa4b6b;  /* fallback for old browsers */
		background: -webkit-linear-gradient(to right, #3b8d99, #6b6b83, #aa4b6b);  /* Chrome 10-25, Safari 5.1-6 */
		background: linear-gradient(to right, #3b8d99, #6b6b83, #aa4b6b); /* W3C, IE 10+/ Edge, Firefox 16+, Chrome 26+, Opera 12+, Safari 7+ */
		-webkit-background-clip: text;
		-webkit-text-fill-color: transparent;
		font-weight: 600;
		font-size: x-large !important;
	}
	
	.footer {
		display: flex;
		position: fixed;
		bottom: 0;
		background-color: #3d4146; /*aliceblue*/
		color: black;
		min-width: 600px;
		padding: 2px;
		margin-bottom: 10px;
		box-shadow: rgba(0, 0, 0, 0.16) 0px 3px 6px, rgba(0, 0, 0, 0.23) 0px 3px 6px;
	}

	#container{
		min-height:100%;
   		position:relative;
		margin-bottom: 100px;
	}

	.terminal{
		display: flex;
		background-color: #000;
		color:#fff;
		flex-wrap: nowrap;
		align-items: baseline;
		width: 100%;
		font-size: medium;
	}

	.terminal_element{
		padding: 12px;
	}

	.terminal_element_left{
		padding-right: 5px;
	}

	.terminal_code{
		flex-grow: 1;
		text-align: left;
		padding-left: 0 !important;
	}

	.terminal_element_right:hover{
		background-color: #212529;
		cursor: pointer; 
	}

	.clearfix::after {
		content: "";
		clear: both;
		display: table;
	}

	i{
		display: inline-block;
	}

	.dropzone_show{
		display: block;
	}

	.dropzone_hide{
		display: none;
	}

	.overlay {
		position: fixed; /* Stay in place */
		z-index: 1; /* Sit on top */
		left: 0;
		top: 0;
		width: 100%; /* Full width */
		height: 100%; /* Full height */
		overflow: auto; /* Enable scroll if needed */
		background-color: rgb(0,0,0); /* Fallback color */
		background-color: rgba(0,0,0,0.4); /* Black w/ opacity */
	}

	.unselectable_text{
		-webkit-user-select: none;  /* Chrome all / Safari all */
		-moz-user-select: none;     /* Firefox all */
		-ms-user-select: none;      /* IE 10+ */
		user-select: none;  
	}

	.key_emboss{
		background-color: aliceblue;
		border-radius: 4px;
		padding: 0.1em 0.2em;
		border-top: 2px solid rgba(255, 255, 255, 0.5);
		border-left: 2px solid rgba(255, 255, 255, 0.5);
		border-right: 2px solid rgba(0, 0, 0, 0.2);
		border-bottom: 2px solid rgba(0, 0, 0, 0.2);
		color: #555;
		display: inline-block;
		font-size: smaller;
	}

	.key_emboss_combine_left{
		margin-right: 2px;
	}

	.key_emboss_combine_right{
		margin-left: 2px;
	}

</style>