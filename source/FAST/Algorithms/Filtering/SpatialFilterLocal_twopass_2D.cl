__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;  //border padding
//__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_MIRRORED_REPEAT | CLK_FILTER_NEAREST; // mirrored padding

__kernel void FilteringLocalMemory(
    __read_only image2d_t input,
    __constant float * mask,
    __local float * sharedMem,
    __write_only image2d_t output)
{
    // TODO check if border pic by using get_group_id()?
    // image size x and y, parameters? definable?
    const int2 pos = { get_global_id(0), get_global_id(1) };
    const int globalX = get_global_id(0);
    const int globalY = get_global_id(1);
    const int localX = get_local_id(0);
    const int localY = get_local_id(1);
    //const int localSizeX = get_local_size(0);
    //const int localSizeY = get_local_size(1);
    //const int localSizeTot = LOCAL_SIZE_X * LOCAL_SIZE_Y;
    //const int localWidth = LOCAL_SIZE_X + 2 * HALF_FILTER_SIZE + 1;
    //const int localHeight = LOCAL_SIZE_Y + 2 * HALF_FILTER_SIZE;
    //const int localTot = localWidth * localHeight;
    // todo define these as build params or sth!
    const int localFetchIndex = localX + localY * LOCAL_WIDTH;//LOCAL_SIZE_X; // ja continuelig
    //const int localIndex = localX + localY * LOCAL_SIZE_X;
    const int2 offset = { HALF_FILTER_SIZE, HALF_FILTER_SIZE };
    int dataType = DATA_TYPE; // get_image_channel_data_type(input);
    if (dataType == CLK_FLOAT) {
        sharedMem[localFetchIndex] = read_imagef(input, sampler, pos - offset).x;
    }
    else if (dataType == CLK_UNSIGNED_INT8 || dataType == CLK_UNSIGNED_INT16) {
        sharedMem[localFetchIndex] = read_imageui(input, sampler, pos - offset).x;
    }
    else {
        sharedMem[localFetchIndex] = read_imagei(input, sampler, pos - offset).x;
    }
    if (localY == 0){ //(localX == 0 && localY == 0){
        //fetch remaining
        // left overs for rows + zero position of local region 
        //int posX = (localSizeTot % LOCAL_WIDTH) + (globalX - localX)
        int2 posBase = { (globalX - localX - HALF_FILTER_SIZE), (globalY - localY - HALF_FILTER_SIZE) };
        int localIndex;
        int x;
        for (int y = 0; y < LOCAL_HEIGHT; y++){
            if (y < LOCAL_SIZE_Y) x = localX + LOCAL_SIZE_X; //{ //x = LOCAL_SIZE_X;
            else x = localX;//x = 0;
            //x = 0;
            //for (; x < LOCAL_WIDTH; x++){
            if (x < LOCAL_WIDTH){ //continue;
                int2 offset2 = { x, y };
                localIndex = x + y * LOCAL_WIDTH;
                if (dataType == CLK_FLOAT) {
                    sharedMem[localIndex] = read_imagef(input, sampler, posBase + offset2).x;
                }
                else if (dataType == CLK_UNSIGNED_INT8 || dataType == CLK_UNSIGNED_INT16) {
                    sharedMem[localIndex] = read_imageui(input, sampler, posBase + offset2).x;
                }
                else {
                    sharedMem[localIndex] = read_imagei(input, sampler, posBase + offset2).x;
                }
            }
            if (x + LOCAL_SIZE_X < LOCAL_WIDTH){
                x = x + LOCAL_SIZE_X;
                int2 offset2 = { x, y };
                localIndex = x + y * LOCAL_WIDTH;
                if (dataType == CLK_FLOAT) {
                    sharedMem[localIndex] = read_imagef(input, sampler, posBase + offset2).x;
                }
                else if (dataType == CLK_UNSIGNED_INT8 || dataType == CLK_UNSIGNED_INT16) {
                    sharedMem[localIndex] = read_imageui(input, sampler, posBase + offset2).x;
                }
                else {
                    sharedMem[localIndex] = read_imagei(input, sampler, posBase + offset2).x;
                }
            }
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    //int localReadIndex = (localX + HALF_FILTER_SIZE) + ((localY + HALF_FILTER_SIZE)*LOCAL_WIDTH);
    //int localReadIndex = (localX + HALF_FILTER_SIZE - 1) + ((localY + HALF_FILTER_SIZE - 1)*LOCAL_WIDTH);
    //float sum = sharedMem[localReadIndex];
    
    float sum = 0.0f;
    int index = localX + localY * LOCAL_WIDTH; //was global?
    //int localReadIndex;
    //int localReadYAdd;
    //int localReadXAdd = localX + HALF_FILTER_SIZE;
    int ySizeAdd = 0;
    for (int y = -HALF_FILTER_SIZE; y <= HALF_FILTER_SIZE; y++){
        //ySizeAdd = (y + HALF_FILTER_SIZE)*FILTER_SIZE;// +HALF_FILTER_SIZE; // = y*FS + (HFS*FS) + HFS
        //localReadYAdd = ((localY + y + HALF_FILTER_SIZE)*LOCAL_WIDTH) + localReadXAdd;
        for (int x = -HALF_FILTER_SIZE; x <= HALF_FILTER_SIZE; x++){
            //int offset = { x, y };
            //index = (HALF_FILTER_SIZE + globalX + x) + ((HALF_FILTER_SIZE + globalY + y) * LOCAL_SIZE_X);
            //localReadIndex = x + localReadYAdd;
            //sum += sharedMem[x + localReadYAdd] * mask[x + ySizeAdd];// halfSize + (y + halfSize)*maskSize];
            sum += sharedMem[index++] * mask[ySizeAdd++];// x + ySizeAdd];
            // localX - x + (localY - y)*LOCAL_SIZE_X
            //index++;
        }
        index += LOCAL_WIDTH - FILTER_SIZE;
        //ySizeAdd++;// = FILTER_SIZE
        //index += LOCAL_MEM_PAD; //?
    }
    //int imSizeX = get_image_width(input);
    //int imSizeY = get_image_height(input);
    if (IMAGE_WIDTH > globalX && IMAGE_HEIGHT > globalY){
        int outputDataType = DATA_TYPE;// get_image_channel_data_type(output);
        if (outputDataType == CLK_FLOAT) {
            write_imagef(output, pos, sum);
        }
        else if (outputDataType == CLK_UNSIGNED_INT8 || outputDataType == CLK_UNSIGNED_INT16) {
            write_imageui(output, pos, round(sum));
        }
        else {
            write_imagei(output, pos, round(sum));
        }
    }
}

