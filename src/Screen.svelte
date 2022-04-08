<script>
    import { onMount } from 'svelte';

    let canvas;
    let ctx;
    let scale = 100;
    let isControlPressed = false;
    let drag = false;
    let imageObj = null;
    export let srcProp = "./screen1.png";
    export let rectW;
    export let rectH;
    export let rectStartX;
    export let rectStartY;
    export let screenW;
    export let screenH;

    $: innerHeight = 0
    
    onMount(() => {
        ctx = canvas.getContext('2d');
        ctx.imageSmoothingEnabled = false;
        loadImg(srcProp);
    });

    $: loadImg(srcProp)

    function loadImg(imgSrc){
        var image = new Image();
        image.src = imgSrc;
        image.onload = function() {
            canvas.width = image.width;
            screenW = image.width;
            canvas.height = image.height;
            screenH = image.height;

            ctx.drawImage(image, 0, 0);
            imageObj = image;

            if ((innerHeight-150) < image.height){
                scale = (innerHeight-150)*100/image.height;
            }else{
                scale = 100;
            }
        };
        rectW = 0;
        rectH = 0;
        rectStartX = 0;
        rectStartY = 0;
    }

    function updateScale(e){
        if (!isControlPressed){
            return;
        }
        
        var delta = e.wheelDelta ? e.wheelDelta : -e.detail;
        if (delta < 0){
            if (scale >= 10){
                scale -= 5; 
            }
        }else{
            if (scale < 100){
                scale += 5;
            }
        }
    }

    function handleKeydown(e){
        if (e.keyCode == 16){
            
            e.preventDefault();
            isControlPressed = true;
        }
    }

    function handleKeyup(e){
        if (e.keyCode == 16){
            isControlPressed = false;
        }
    }

    function drawDown(e){
        if (e.button == 0){
            rectStartX = canvas.width*e.offsetX/canvas.getBoundingClientRect().width;
            rectStartY = canvas.height*e.offsetY/canvas.getBoundingClientRect().height;

            drag = true;
        }
    }

    function drawMove(e){
        if (drag) {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.drawImage(imageObj, 0, 0);
            rectW = (canvas.width*e.offsetX/canvas.getBoundingClientRect().width) - rectStartX;
            rectH = (canvas.height*e.offsetY/canvas.getBoundingClientRect().height) - rectStartY;
            ctx.shadowColor = '#d53';
            ctx.shadowBlur = 20;
            ctx.lineJoin = 'bevel';
            ctx.lineWidth = 15;
            ctx.strokeStyle = '#38f';
            ctx.fillStyle = 'rgba(225,225,225,0.5)';
            ctx.fillRect(rectStartX, rectStartY, rectW, rectH);
        }
    }

    function drawUp(e){
        drag = false;
    }

    function drawOut(e){
        
    }

</script>

<svelte:window bind:innerHeight on:DOMMouseScroll={updateScale} on:mousewheel={updateScale} on:keydown={handleKeydown} on:keyup={handleKeyup}/>

<div class="screen">
    <!-- svelte-ignore a11y-mouse-events-have-key-events -->
    <canvas
        on:mousedown={drawDown}
        on:mousemove={drawMove}
        on:mouseup={drawUp}
        on:mouseout={drawOut}
        style='width: {scale}%; height: {scale};'
        bind:this={canvas}
        width={32}
        height={32}
    ></canvas>
</div>

<style>
    .screen{
        display: flex;
        justify-content:center;
    }

    canvas {
		background-color: #666;
        cursor: crosshair;
	}
</style>