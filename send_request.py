import base64
import requests

URL_TEXT = "https://newserver-n8n.5bxr29.easypanel.host/webhook-test/099aafb3-27de-473e-a1a7-934d77943d3f" # Set the API URL

def ask_n8n(prompt:str) -> dict:
    """
    Send a request to the n8n API
    Args:
        prompt (str): Prompt to send to the API
    Returns:
        dict: Response from the API
    """
    headers = {
        "Content-Type": "application/json",
    }
    data = {
        "message": prompt,
    }
    response = requests.post(URL_TEXT, headers=headers, json=data) # Send the request
    return response.json() # Return the response

def ask_n8n_with_imagefile(filename: str, prompt: str) -> dict:
    """
    Send a request to the n8n API with an image file
    Args:
        filename (str): Name of the file to send
        prompt (str): Prompt to send to the API
    Returns:
        dict: Response from the API
    """
    headers = {"Content-Type": "application/json"}
    
    with open(filename, "rb") as f:
        img_b64 = base64.b64encode(f.read()).decode("utf-8")
        
    data = {
        "image_base64": img_b64,
        "prompt": prompt
    }
    
    response = requests.post(URL_TEXT, headers=headers, json=data)
    response.raise_for_status()
    return response.json()

    
if __name__ == "__main__":
    # print(ask_n8n("hola"))
    print(ask_n8n_with_imagefile("images-test/test1.jpg", "que ves en la imagen?"))
    